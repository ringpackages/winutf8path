/*
**  WinUTF8Path - UTF-8 file paths on MS-Windows for the Ring Programming Language
**
**  Ring strings are UTF-8, but on MS-Windows the ANSI C functions (stat, rename, remove, _chdir, ...)
**  and the ANSI Windows API (FindFirstFileA, ...) expect paths in the current ANSI code page
**  So paths that contain characters outside this code page (Cyrillic, Arabic, Chinese, ...) fail
**
**  On MS-Windows these functions convert UTF-8 to UTF-16 and call the wide character API (...W)
**  Then convert the results (file names, folders, environment values) back to UTF-8
**  Strings that are not valid UTF-8 are converted using the ANSI code page (like the Ring VM)
**  Long paths (>= 248 characters) get the \\?\ prefix automatically
**
**  On Linux, macOS, etc. the functions use the ANSI C/POSIX functions (paths are already UTF-8)
**  They have the same behavior (parameters, return values and errors) as the Ring functions
**
**  The library is self-contained: it doesn't call the file/OS functions of the Ring VM
**
*/

#ifdef _WIN32
	/* _wfopen() is used (like the Ring VM) */
	#ifndef _CRT_SECURE_NO_WARNINGS
		#define _CRT_SECURE_NO_WARNINGS
	#endif
#endif

#include "ring.h"

#ifdef _WIN32
	#include <windows.h>
	#include <shellapi.h>
	#include <wchar.h>
#else
	#include <sys/types.h>
	#include <sys/stat.h>
	#include <unistd.h>
	#include <dirent.h>
	#ifdef __MACH__
		#include <mach-o/dyld.h>
	#endif
	#ifdef __FreeBSD__
		#include <sys/sysctl.h>
	#endif
#endif

/* Error messages (Same as the Ring VM) */

#ifndef RING_VM_ERROR_CANTOPENFILE
	#define RING_VM_ERROR_CANTOPENFILE "Error (R35) : Can't create/open the file"
#endif

#ifndef RING_VM_ERROR_TEMPFILENAME
	#define RING_VM_ERROR_TEMPFILENAME "Error (R39) : Error occurred creating unique filename."
#endif

#ifndef RING_API_BADDIRECTORY
	#define RING_API_BADDIRECTORY "Error, Couldn't open the directory"
#endif

#ifndef RING_PATHTYPE_NOTKNOWN
	#define RING_PATHTYPE_NOTKNOWN -1
	#define RING_PATHTYPE_NOTFOUND 0
	#define RING_PATHTYPE_FILE 1
	#define RING_PATHTYPE_DIR 2
#endif

#define WUTF8_ERROR_OOM "Error, WinUTF8Path: Out of memory"
#define WUTF8_ERROR_NUL "Error, WinUTF8Path: The string contains the NUL character"

/* Paths with this length (or more) get the \\?\ prefix (CreateDirectoryW limit is MAX_PATH - 12) */
#define WUTF8_LONGPATH 248

/* Buffer size used by w_copyfile() */
#define WUTF8_COPYBUFFER 65536

/*
**  Check the parameters: nCount strings
**  Paths and names can't contain the NUL character (The C functions stop at it, so they would use another path)
**  nData is a parameter that can contain NUL (data written to a file), or 0
*/
static int wutf8_checkparameters(void *pPointer, int nCount, int nData) {
	int x;
	if (RING_API_PARACOUNT != nCount) {
		if (nCount == 0) {
			RING_API_ERROR(RING_API_BADPARACOUNT);
		} else if (nCount == 1) {
			RING_API_ERROR(RING_API_MISS1PARA);
		} else {
			RING_API_ERROR(RING_API_MISS2PARA);
		}
		return RING_FALSE;
	}
	for (x = 1; x <= nCount; x++) {
		if (!RING_API_ISSTRING(x)) {
			RING_API_ERROR(RING_API_BADPARATYPE);
			return RING_FALSE;
		}
	}
	for (x = 1; x <= nCount; x++) {
		if ((x != nData) && (strlen(RING_API_GETSTRING(x)) != (size_t)RING_API_GETSTRINGSIZE(x))) {
			RING_API_ERROR(WUTF8_ERROR_NUL);
			return RING_FALSE;
		}
	}
	return RING_TRUE;
}

#define wutf8_checkstrings(pPointer, nCount) wutf8_checkparameters(pPointer, nCount, 0)

#ifdef _WIN32

/* UTF-8 <-> UTF-16 conversion (MS-Windows) */

static wchar_t *wutf8_widen(const char *cStr) {
	int nSize, nLen;
	UINT nCodePage;
	wchar_t *pWide;
	nSize = (int)strlen(cStr);
	nCodePage = CP_UTF8;
	nLen = 0;
	if (nSize > 0) {
		nLen = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, cStr, nSize, NULL, 0);
		if (nLen == 0) {
			/* Not valid UTF-8, use the ANSI code page (like the Ring VM) */
			nCodePage = CP_ACP;
			nLen = MultiByteToWideChar(CP_ACP, 0, cStr, nSize, NULL, 0);
			if (nLen == 0) {
				return NULL;
			}
		}
	}
	pWide = (wchar_t *)malloc(sizeof(wchar_t) * (nLen + 1));
	if (pWide == NULL) {
		return NULL;
	}
	if (nLen > 0) {
		MultiByteToWideChar(nCodePage, 0, cStr, nSize, pWide, nLen);
	}
	pWide[nLen] = L'\0';
	return pWide;
}

static char *wutf8_narrow(const wchar_t *pWide) {
	int nLen;
	char *cStr;
	nLen = WideCharToMultiByte(CP_UTF8, 0, pWide, -1, NULL, 0, NULL, NULL);
	if (nLen <= 0) {
		nLen = 1;
	}
	cStr = (char *)malloc(nLen);
	if (cStr == NULL) {
		return NULL;
	}
	cStr[0] = '\0';
	WideCharToMultiByte(CP_UTF8, 0, pWide, -1, cStr, nLen, NULL, NULL);
	return cStr;
}

/*
**  If the full path is long, return the full path with the \\?\ prefix (or \\?\UNC\ for network paths)
**  GetFullPathNameW() normalizes the path like the Win32 functions (/ becomes \, . and .. are removed,
**  spaces and dots at the end are removed), so the long path means the same file as the short path
**  Short paths are not changed (relative paths stay relative)
**  pWide is owned by the function: the result is pWide or a new string (pWide is freed), NULL if out of memory
*/
static wchar_t *wutf8_longpath(wchar_t *pWide) {
	wchar_t *pFull;
	size_t nFull;
	DWORD nSize, nResult;
	/* Already a \\?\ path or a device path (\\.\) */
	if ((wcsncmp(pWide, L"\\\\?\\", 4) == 0) || (wcsncmp(pWide, L"\\\\.\\", 4) == 0)) {
		return pWide;
	}
	/* The size of the full path (with the null character), a relative path could be long too */
	nSize = GetFullPathNameW(pWide, 0, NULL, NULL);
	if ((nSize == 0) || (nSize - 1 < WUTF8_LONGPATH)) {
		return pWide;
	}
	/* Long path: get the full path then add the prefix */
	pFull = (wchar_t *)malloc(sizeof(wchar_t) * (nSize + 8));
	if (pFull == NULL) {
		free(pWide);
		return NULL;
	}
	nResult = GetFullPathNameW(pWide, nSize, pFull + 8, NULL);
	if ((nResult == 0) || (nResult >= nSize)) {
		/* Failed (or the current folder changed and the buffer is small) */
		free(pFull);
		return pWide;
	}
	free(pWide);
	nFull = wcslen(pFull + 8);
	if ((wcsncmp(pFull + 8, L"\\\\?\\", 4) == 0) || (wcsncmp(pFull + 8, L"\\\\.\\", 4) == 0)) {
		memmove(pFull, pFull + 8, sizeof(wchar_t) * (nFull + 1));
		return pFull;
	}
	if (wcsncmp(pFull + 8, L"\\\\", 2) == 0) {
		/* \\server\share\... becomes \\?\UNC\server\share\... */
		memmove(pFull + 8, pFull + 10, sizeof(wchar_t) * (nFull - 1));
		memcpy(pFull, L"\\\\?\\UNC\\", sizeof(wchar_t) * 8);
		return pFull;
	}
	memmove(pFull + 4, pFull + 8, sizeof(wchar_t) * (nFull + 1));
	memcpy(pFull, L"\\\\?\\", sizeof(wchar_t) * 4);
	return pFull;
}

/* Convert a Ring (UTF-8) path to a wide path, add the optional suffix, then use wutf8_longpath() */
static wchar_t *wutf8_path(const char *cPath, const wchar_t *cSuffix) {
	wchar_t *pWide, *pNew;
	size_t nLen, nSuffix;
	pWide = wutf8_widen(cPath);
	if (pWide == NULL) {
		return NULL;
	}
	if (cSuffix != NULL) {
		nLen = wcslen(pWide);
		nSuffix = wcslen(cSuffix);
		pNew = (wchar_t *)realloc(pWide, sizeof(wchar_t) * (nLen + nSuffix + 1));
		if (pNew == NULL) {
			free(pWide);
			return NULL;
		}
		pWide = pNew;
		memcpy(pWide + nLen, cSuffix, sizeof(wchar_t) * (nSuffix + 1));
	}
	return wutf8_longpath(pWide);
}

/* Create a folder, the path (pWide) is not changed */
static int wutf8_createfolder(const wchar_t *pWide) {
	wchar_t *pPath;
	size_t nLen;
	nLen = wcslen(pWide);
	pPath = (wchar_t *)malloc(sizeof(wchar_t) * (nLen + 1));
	if (pPath == NULL) {
		return RING_FALSE;
	}
	memcpy(pPath, pWide, sizeof(wchar_t) * (nLen + 1));
	pPath = wutf8_longpath(pPath);
	if (pPath == NULL) {
		return RING_FALSE;
	}
	CreateDirectoryW(pPath, NULL);
	free(pPath);
	return RING_TRUE;
}

static void wutf8_retwide(void *pPointer, const wchar_t *pWide) {
	char *cStr;
	cStr = wutf8_narrow(pWide);
	if (cStr == NULL) {
		RING_API_ERROR(WUTF8_ERROR_OOM);
		return;
	}
	RING_API_RETSTRING(cStr);
	free(cStr);
}

/* Get the path parameter (nPara) as a wide path, raise an error if this is not possible */
static wchar_t *wutf8_getpath(void *pPointer, int nPara, const wchar_t *cSuffix) {
	wchar_t *pWide;
	pWide = wutf8_path(RING_API_GETSTRING(nPara), cSuffix);
	if (pWide == NULL) {
		RING_API_ERROR(WUTF8_ERROR_OOM);
	}
	return pWide;
}

/*
**  Get the attributes (and the file size) of a path
**  Symbolic links/Junctions are followed (like stat())
**  Returns RING_FALSE if the path doesn't exist
*/
static int wutf8_attributes(const wchar_t *pPath, DWORD *pAttributes, RING_LONGLONG *pSize) {
	WIN32_FILE_ATTRIBUTE_DATA oData;
	BY_HANDLE_FILE_INFORMATION oInfo;
	WIN32_FIND_DATAW oFind;
	HANDLE hFile;
	if (!GetFileAttributesExW(pPath, GetFileExInfoStandard, &oData)) {
		/* Files opened by the system (like pagefile.sys), stat() uses FindFirstFile() too */
		if ((GetLastError() != ERROR_SHARING_VIOLATION) || (wcspbrk(pPath, L"*?") != NULL)) {
			return RING_FALSE;
		}
		hFile = FindFirstFileW(pPath, &oFind);
		if (hFile == INVALID_HANDLE_VALUE) {
			return RING_FALSE;
		}
		FindClose(hFile);
		oData.dwFileAttributes = oFind.dwFileAttributes;
		oData.nFileSizeHigh = oFind.nFileSizeHigh;
		oData.nFileSizeLow = oFind.nFileSizeLow;
	}
	*pAttributes = oData.dwFileAttributes;
	*pSize = (((RING_LONGLONG)oData.nFileSizeHigh) << 32) | (RING_LONGLONG)oData.nFileSizeLow;
	if (oData.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
		hFile = CreateFileW(pPath, FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
				    NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
		if (hFile == INVALID_HANDLE_VALUE) {
			/* Broken link */
			return RING_FALSE;
		}
		if (GetFileInformationByHandle(hFile, &oInfo)) {
			*pAttributes = oInfo.dwFileAttributes;
			*pSize =
			    (((RING_LONGLONG)oInfo.nFileSizeHigh) << 32) | (RING_LONGLONG)oInfo.nFileSizeLow;
		}
		CloseHandle(hFile);
	}
	return RING_TRUE;
}

/* Path type (Same values as getpathtype()) */
static int wutf8_pathtype(const wchar_t *pPath) {
	DWORD nAttributes;
	RING_LONGLONG nSize;
	if (!wutf8_attributes(pPath, &nAttributes, &nSize)) {
		return RING_PATHTYPE_NOTFOUND;
	}
	if (nAttributes & FILE_ATTRIBUTE_DIRECTORY) {
		return RING_PATHTYPE_DIR;
	}
	if (nAttributes & FILE_ATTRIBUTE_DEVICE) {
		return RING_PATHTYPE_NOTKNOWN;
	}
	return RING_PATHTYPE_FILE;
}

/* Path type of the path parameter (nPara), -2 if an error is raised */
static int wutf8_parapathtype(void *pPointer, int nPara) {
	wchar_t *pPath;
	int nType;
	pPath = wutf8_getpath(pPointer, nPara, NULL);
	if (pPath == NULL) {
		return -2;
	}
	nType = wutf8_pathtype(pPath);
	free(pPath);
	return nType;
}

/* The executable file name (wide string, allocated using malloc), NULL on failure */
static wchar_t *wutf8_exefilename(void) {
	DWORD nSize, nLen;
	wchar_t *pName, *pNew;
	nSize = MAX_PATH;
	pName = NULL;
	while (nSize <= 32768 * 2) {
		pNew = (wchar_t *)realloc(pName, sizeof(wchar_t) * nSize);
		if (pNew == NULL) {
			free(pName);
			return NULL;
		}
		pName = pNew;
		nLen = GetModuleFileNameW(NULL, pName, nSize);
		if (nLen == 0) {
			free(pName);
			return NULL;
		}
		if (nLen < nSize) {
			return pName;
		}
		nSize *= 2;
	}
	free(pName);
	return NULL;
}

#else

/* Path type using stat() (Same values as getpathtype()) */
static int wutf8_pathtype(const char *cPath) {
	struct stat oStat;
	if (stat(cPath, &oStat) != 0) {
		return RING_PATHTYPE_NOTFOUND;
	}
	if (S_ISREG(oStat.st_mode)) {
		return RING_PATHTYPE_FILE;
	}
	if (S_ISDIR(oStat.st_mode)) {
		return RING_PATHTYPE_DIR;
	}
	return RING_PATHTYPE_NOTKNOWN;
}

/* Path type of the path parameter (nPara) */
static int wutf8_parapathtype(void *pPointer, int nPara) {
	return wutf8_pathtype(RING_API_GETSTRING(nPara));
}

/* The executable file name (allocated using malloc), NULL on failure */
static char *wutf8_exefilename(void) {
	char *cName;
	#ifdef __MACH__
	uint32_t nSize;
	char *cReal;
	nSize = 0;
	_NSGetExecutablePath(NULL, &nSize);
	cName = (char *)malloc(nSize + 1);
	if (cName == NULL) {
		return NULL;
	}
	if (_NSGetExecutablePath(cName, &nSize) != 0) {
		free(cName);
		return NULL;
	}
	cReal = realpath(cName, NULL);
	if (cReal != NULL) {
		free(cName);
		return cReal;
	}
	return cName;
	#elif defined(__FreeBSD__)
	int aMib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1};
	size_t nSize;
	nSize = 0;
	if (sysctl(aMib, 4, NULL, &nSize, NULL, 0) != 0) {
		return NULL;
	}
	cName = (char *)malloc(nSize + 1);
	if (cName == NULL) {
		return NULL;
	}
	if (sysctl(aMib, 4, cName, &nSize, NULL, 0) != 0) {
		free(cName);
		return NULL;
	}
	cName[nSize] = '\0';
	return cName;
	#else
	size_t nSize;
	ssize_t nLen;
	char *cNew;
	nSize = 256;
	cName = NULL;
	while (nSize <= 1048576) {
		cNew = (char *)realloc(cName, nSize);
		if (cNew == NULL) {
			free(cName);
			return NULL;
		}
		cName = cNew;
		/* readlink() doesn't add the null character */
		nLen = readlink("/proc/self/exe", cName, nSize);
		if (nLen < 0) {
			free(cName);
			return NULL;
		}
		if ((size_t)nLen < nSize) {
			cName[nLen] = '\0';
			return cName;
		}
		nSize *= 2;
	}
	free(cName);
	return NULL;
	#endif
}

#endif

/* read(cFileName) --> cString */
RING_FUNC(ring_w_read) {
	FILE *pFile;
	size_t nCount;
	char *cBuffer;
	int lError;
#ifdef _WIN32
	wchar_t *pPath;
	__int64 nSize;
#else
	long nSize;
	struct stat oStat;
#endif
	if (!wutf8_checkstrings(pPointer, 1)) {
		return;
	}
#ifdef _WIN32
	pPath = wutf8_getpath(pPointer, 1, NULL);
	if (pPath == NULL) {
		return;
	}
	pFile = _wfopen(pPath, L"rb");
	free(pPath);
#else
	pFile = fopen(RING_API_GETSTRING(1), "rb");
#endif
	if (pFile == NULL) {
		RING_API_ERROR(RING_VM_ERROR_CANTOPENFILE);
		return;
	}
#ifndef _WIN32
	/* On Linux fopen() can open a folder (then ftell() could return a wrong size) */
	if ((fstat(fileno(pFile), &oStat) != 0) || S_ISDIR(oStat.st_mode)) {
		fclose(pFile);
		RING_API_ERROR(RING_CANTREADFILE);
		return;
	}
#endif
	lError = RING_TRUE;
	nSize = 0;
#ifdef _WIN32
	if (_fseeki64(pFile, 0, SEEK_END) == 0) {
		nSize = _ftelli64(pFile);
		if ((nSize >= 0) && (nSize < INT_MAX) && (_fseeki64(pFile, 0, SEEK_SET) == 0)) {
			lError = RING_FALSE;
		}
	}
#else
	if (fseek(pFile, 0, SEEK_END) == 0) {
		nSize = ftell(pFile);
		if ((nSize >= 0) && (nSize < INT_MAX) && (fseek(pFile, 0, SEEK_SET) == 0)) {
			lError = RING_FALSE;
		}
	}
#endif
	if (lError) {
		fclose(pFile);
		RING_API_ERROR(RING_CANTREADFILE);
		return;
	}
	RING_API_RETSTRINGSIZE((unsigned int)nSize);
	cBuffer = ring_string_get(RING_API_GETSTRINGRAW);
	nCount = fread(cBuffer, 1, (size_t)nSize, pFile);
	fclose(pFile);
	if (nCount != (size_t)nSize) {
		RING_API_ERROR(RING_CANTREADFILE);
	}
}

/* write(cFileName,cString) */
RING_FUNC(ring_w_write) {
	FILE *pFile;
#ifdef _WIN32
	wchar_t *pPath;
#endif
	/* The second parameter is the data (it can contain the NUL character) */
	if (!wutf8_checkparameters(pPointer, 2, 2)) {
		return;
	}
#ifdef _WIN32
	pPath = wutf8_getpath(pPointer, 1, NULL);
	if (pPath == NULL) {
		return;
	}
	pFile = _wfopen(pPath, L"w+b");
	free(pPath);
#else
	pFile = fopen(RING_API_GETSTRING(1), "w+b");
#endif
	if (pFile == NULL) {
		RING_API_ERROR(RING_VM_ERROR_CANTOPENFILE);
		return;
	}
	fwrite(RING_API_GETSTRING(2), RING_API_GETSTRINGSIZE(2), 1, pFile);
	fflush(pFile);
	fclose(pFile);
}

/* fexists(cFileName) --> 1 if the path is a regular file */
RING_FUNC(ring_w_fexists) {
	int nType;
	if (!wutf8_checkstrings(pPointer, 1)) {
		return;
	}
	nType = wutf8_parapathtype(pPointer, 1);
	if (nType != -2) {
		RING_API_RETNUMBER(nType == RING_PATHTYPE_FILE);
	}
}

/* direxists(cDirPath) --> 1 if the path is a directory */
RING_FUNC(ring_w_direxists) {
	int nType;
	if (!wutf8_checkstrings(pPointer, 1)) {
		return;
	}
	nType = wutf8_parapathtype(pPointer, 1);
	if (nType != -2) {
		RING_API_RETNUMBER(nType == RING_PATHTYPE_DIR);
	}
}

/* getpathtype(cPath) --> 0 (Not Found), 1 (File), 2 (Directory), -1 (Unknown) */
RING_FUNC(ring_w_getpathtype) {
	int nType;
	if (!wutf8_checkstrings(pPointer, 1)) {
		return;
	}
	nType = wutf8_parapathtype(pPointer, 1);
	if (nType != -2) {
		RING_API_RETNUMBER(nType);
	}
}

/* getfilesize(cFilePath) --> nSize or -1 */
RING_FUNC(ring_w_getfilesize) {
#ifdef _WIN32
	wchar_t *pPath;
	DWORD nAttributes;
	RING_LONGLONG nSize;
#else
	struct stat oStat;
#endif
	if (!wutf8_checkstrings(pPointer, 1)) {
		return;
	}
#ifdef _WIN32
	pPath = wutf8_getpath(pPointer, 1, NULL);
	if (pPath == NULL) {
		return;
	}
	if (wutf8_attributes(pPath, &nAttributes, &nSize) &&
	    !(nAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_DEVICE))) {
		RING_API_RETNUMBER((double)nSize);
	} else {
		RING_API_RETNUMBER(-1);
	}
	free(pPath);
#else
	if ((stat(RING_API_GETSTRING(1), &oStat) == 0) && S_ISREG(oStat.st_mode)) {
		RING_API_RETNUMBER((double)oStat.st_size);
	} else {
		RING_API_RETNUMBER(-1);
	}
#endif
}

/* dir(cFolder) --> [ [cName, lIsDir], ... ] */
RING_FUNC(ring_w_dir) {
	List *pList, *pList2;
	RingState *pRingState;
#ifdef _WIN32
	WIN32_FIND_DATAW oData;
	HANDLE hFind;
	wchar_t *pPath;
	char *cName;
	const char *cFolder;
	size_t nFolder;
	DWORD nError;
#else
	DIR *pDir;
	struct dirent *pEntry;
	struct stat oStat;
	const char *cFolder;
	char *cPath;
	size_t nFolder, nName;
	int lIsDir;
#endif
	if (!wutf8_checkstrings(pPointer, 1)) {
		return;
	}
	pRingState = ((VM *)pPointer)->pRingState;
#ifdef _WIN32
	/* Folder + \*.* (Don't add \ if the folder ends with \ or / ) */
	cFolder = RING_API_GETSTRING(1);
	nFolder = strlen(cFolder);
	if ((nFolder > 0) && ((cFolder[nFolder - 1] == '\\') || (cFolder[nFolder - 1] == '/'))) {
		pPath = wutf8_getpath(pPointer, 1, L"*.*");
	} else {
		pPath = wutf8_getpath(pPointer, 1, L"\\*.*");
	}
	if (pPath == NULL) {
		return;
	}
	hFind = FindFirstFileW(pPath, &oData);
	nError = GetLastError();
	free(pPath);
	if (hFind == INVALID_HANDLE_VALUE) {
		if (nError == ERROR_FILE_NOT_FOUND) {
			/* The folder exists, but it's empty (for example the root of an empty drive) */
			pList = RING_API_NEWLIST;
			RING_API_RETLISTBYREF(pList);
		} else {
			RING_API_ERROR(RING_API_BADDIRECTORY);
		}
		return;
	}
	pList = RING_API_NEWLIST;
	do {
		if ((wcscmp(oData.cFileName, L".") == 0) || (wcscmp(oData.cFileName, L"..") == 0)) {
			continue;
		}
		cName = wutf8_narrow(oData.cFileName);
		if (cName == NULL) {
			continue;
		}
		pList2 = ring_list_newlist_gc(pRingState, pList);
		ring_list_addstring_gc(pRingState, pList2, cName);
		free(cName);
		if (oData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
			ring_list_adddouble_gc(pRingState, pList2, RING_TRUEF);
		} else {
			ring_list_adddouble_gc(pRingState, pList2, RING_ZEROF);
		}
	} while (FindNextFileW(hFind, &oData));
	FindClose(hFind);
#else
	cFolder = RING_API_GETSTRING(1);
	pDir = opendir(cFolder);
	if (pDir == NULL) {
		RING_API_ERROR(RING_API_BADDIRECTORY);
		return;
	}
	pList = RING_API_NEWLIST;
	nFolder = strlen(cFolder);
	while ((pEntry = readdir(pDir)) != NULL) {
		if ((strcmp(pEntry->d_name, ".") == 0) || (strcmp(pEntry->d_name, "..") == 0)) {
			continue;
		}
		/* Folder + / + Name (stat() follows links, like the Ring VM) */
		nName = strlen(pEntry->d_name);
		lIsDir = RING_FALSE;
		cPath = (char *)malloc(nFolder + nName + 2);
		if (cPath != NULL) {
			memcpy(cPath, cFolder, nFolder);
			cPath[nFolder] = '/';
			memcpy(cPath + nFolder + 1, pEntry->d_name, nName + 1);
			if ((stat(cPath, &oStat) == 0) && S_ISDIR(oStat.st_mode)) {
				lIsDir = RING_TRUE;
			}
			free(cPath);
		}
		pList2 = ring_list_newlist_gc(pRingState, pList);
		ring_list_addstring_gc(pRingState, pList2, pEntry->d_name);
		ring_list_adddouble_gc(pRingState, pList2, lIsDir ? RING_TRUEF : RING_ZEROF);
	}
	closedir(pDir);
#endif
	RING_API_RETLISTBYREF(pList);
}

/* rename(cOldPath,cNewPath) --> 0 (Success) or -1 */
RING_FUNC(ring_w_rename) {
#ifdef _WIN32
	wchar_t *pOld, *pNew;
	size_t nLen;
#endif
	if (!wutf8_checkstrings(pPointer, 2)) {
		return;
	}
#ifdef _WIN32
	pOld = wutf8_getpath(pPointer, 1, NULL);
	if (pOld == NULL) {
		return;
	}
	pNew = wutf8_getpath(pPointer, 2, NULL);
	if (pNew == NULL) {
		free(pOld);
		return;
	}
	/*
	**  New path that ends with a separator: like rename() on Linux, the old path must be a folder
	**  The separators are removed (Else Windows would keep spaces/dots at the end of the new name)
	*/
	nLen = wcslen(pNew);
	if ((nLen > 1) && ((pNew[nLen - 1] == L'\\') || (pNew[nLen - 1] == L'/'))) {
		if (wutf8_pathtype(pOld) != RING_PATHTYPE_DIR) {
			RING_API_RETNUMBER(-1);
			free(pOld);
			free(pNew);
			return;
		}
		while ((nLen > 1) && ((pNew[nLen - 1] == L'\\') || (pNew[nLen - 1] == L'/'))) {
			pNew[--nLen] = L'\0';
		}
	}
	/* Same flags used by rename() in the Visual C/C++ runtime */
	if (MoveFileExW(pOld, pNew, MOVEFILE_COPY_ALLOWED)) {
		RING_API_RETNUMBER(0);
	} else {
		RING_API_RETNUMBER(-1);
	}
	free(pOld);
	free(pNew);
#else
	RING_API_RETNUMBER(rename(RING_API_GETSTRING(1), RING_API_GETSTRING(2)) == 0 ? 0 : -1);
#endif
}

/* remove(cFileName) (Like the Ring function, nothing is returned) */
RING_FUNC(ring_w_remove) {
#ifdef _WIN32
	wchar_t *pPath;
#endif
	if (!wutf8_checkstrings(pPointer, 1)) {
		return;
	}
#ifdef _WIN32
	pPath = wutf8_getpath(pPointer, 1, NULL);
	if (pPath == NULL) {
		return;
	}
	DeleteFileW(pPath);
	free(pPath);
#else
	remove(RING_API_GETSTRING(1));
#endif
}

/* tempname() --> cFileName */
#if defined(_WIN32) && !defined(L_tmpnam_s)
	#define L_tmpnam_s L_tmpnam
#endif

RING_FUNC(ring_w_tempname) {
#ifdef _WIN32
	wchar_t cTemp[L_tmpnam_s + 1];
#else
	char cTemp[] = "/tmp/ringtempXXXXXX";
	int nFile;
#endif
	if (RING_API_PARACOUNT != 0) {
		RING_API_ERROR(RING_API_BADPARACOUNT);
		return;
	}
#ifdef _WIN32
	#ifdef _MSC_VER
	if (_wtmpnam_s(cTemp, L_tmpnam_s) != 0) {
		RING_API_ERROR(RING_VM_ERROR_TEMPFILENAME);
		return;
	}
	#else
	if (_wtmpnam(cTemp) == NULL) {
		RING_API_ERROR(RING_VM_ERROR_TEMPFILENAME);
		return;
	}
	#endif
	wutf8_retwide(pPointer, cTemp);
#else
	/* Like the Ring VM on Linux, the file is created (no other program can get the same name) */
	nFile = mkstemp(cTemp);
	if (nFile == -1) {
		RING_API_ERROR(RING_VM_ERROR_TEMPFILENAME);
		return;
	}
	close(nFile);
	RING_API_RETSTRING(cTemp);
#endif
}

/* chdir(cFolder) --> 0 (Success) or -1 */
RING_FUNC(ring_w_chdir) {
#ifdef _WIN32
	wchar_t *pPath;
#endif
	if (!wutf8_checkstrings(pPointer, 1)) {
		return;
	}
#ifdef _WIN32
	pPath = wutf8_getpath(pPointer, 1, NULL);
	if (pPath == NULL) {
		return;
	}
	if (SetCurrentDirectoryW(pPath)) {
		RING_API_RETNUMBER(0);
	} else {
		RING_API_RETNUMBER(-1);
	}
	free(pPath);
#else
	RING_API_RETNUMBER(chdir(RING_API_GETSTRING(1)) == 0 ? 0 : -1);
#endif
}

/* currentdir() --> cFolder */
RING_FUNC(ring_w_currentdir) {
#ifdef _WIN32
	DWORD nSize, nLen;
	wchar_t *pDir;
#else
	size_t nSize;
	char *cDir, *cNew;
#endif
	if (RING_API_PARACOUNT != 0) {
		RING_API_ERROR(RING_API_BADPARACOUNT);
		return;
	}
#ifdef _WIN32
	/* Repeat if the current folder changed (by another thread) and the buffer became small */
	nSize = GetCurrentDirectoryW(0, NULL);
	while (nSize > 0) {
		pDir = (wchar_t *)malloc(sizeof(wchar_t) * (nSize + 1));
		if (pDir == NULL) {
			RING_API_ERROR(WUTF8_ERROR_OOM);
			return;
		}
		nLen = GetCurrentDirectoryW(nSize + 1, pDir);
		if ((nLen > 0) && (nLen <= nSize)) {
			pDir[nLen] = L'\0';
			wutf8_retwide(pPointer, pDir);
			free(pDir);
			return;
		}
		free(pDir);
		nSize = nLen;
	}
	RING_API_RETSTRING(RING_CSTR_EMPTY);
#else
	nSize = 256;
	cDir = NULL;
	while (nSize <= 1048576) {
		cNew = (char *)realloc(cDir, nSize);
		if (cNew == NULL) {
			break;
		}
		cDir = cNew;
		if (getcwd(cDir, nSize) != NULL) {
			RING_API_RETSTRING(cDir);
			free(cDir);
			return;
		}
		if (errno != ERANGE) {
			break;
		}
		nSize *= 2;
	}
	free(cDir);
	RING_API_RETSTRING(RING_CSTR_EMPTY);
#endif
}

/* exefilename() --> cFileName */
RING_FUNC(ring_w_exefilename) {
#ifdef _WIN32
	wchar_t *pName;
#else
	char *cName;
#endif
	if (RING_API_PARACOUNT != 0) {
		RING_API_ERROR(RING_API_BADPARACOUNT);
		return;
	}
#ifdef _WIN32
	pName = wutf8_exefilename();
	if (pName != NULL) {
		wutf8_retwide(pPointer, pName);
		free(pName);
	}
#else
	cName = wutf8_exefilename();
	if (cName != NULL) {
		RING_API_RETSTRING(cName);
		free(cName);
	}
#endif
}

/* exefolder() --> cFolder (ends with the path separator) */
RING_FUNC(ring_w_exefolder) {
	int x;
#ifdef _WIN32
	wchar_t *pName;
#else
	char *cName;
#endif
	if (RING_API_PARACOUNT != 0) {
		RING_API_ERROR(RING_API_BADPARACOUNT);
		return;
	}
#ifdef _WIN32
	pName = wutf8_exefilename();
	if (pName == NULL) {
		RING_API_RETSTRING(RING_CSTR_EMPTY);
		return;
	}
	for (x = (int)wcslen(pName) - 1; x >= 0; x--) {
		if ((pName[x] == L'\\') || (pName[x] == L'/')) {
			break;
		}
	}
	pName[x + 1] = L'\0';
	wutf8_retwide(pPointer, pName);
	free(pName);
#else
	cName = wutf8_exefilename();
	if (cName == NULL) {
		RING_API_RETSTRING(RING_CSTR_EMPTY);
		return;
	}
	for (x = (int)strlen(cName) - 1; x >= 0; x--) {
		if (cName[x] == '/') {
			break;
		}
	}
	cName[x + 1] = '\0';
	RING_API_RETSTRING(cName);
	free(cName);
#endif
}

/* system(cCommand) --> nExitCode */
RING_FUNC(ring_w_system) {
#ifdef _WIN32
	wchar_t *pCommand;
#endif
	if (!wutf8_checkstrings(pPointer, 1)) {
		return;
	}
#ifdef _WIN32
	pCommand = wutf8_widen(RING_API_GETSTRING(1));
	if (pCommand == NULL) {
		RING_API_ERROR(WUTF8_ERROR_OOM);
		return;
	}
	RING_API_RETNUMBER((double)_wsystem(pCommand));
	free(pCommand);
#else
	RING_API_RETNUMBER((double)system(RING_API_GETSTRING(1)));
#endif
}

/* sysget(cVariable) --> cValue ("" if not found) */
RING_FUNC(ring_w_sysget) {
#ifdef _WIN32
	wchar_t *pName, *pValue;
	DWORD nSize, nLen;
#else
	const char *cValue;
#endif
	if (!wutf8_checkstrings(pPointer, 1)) {
		return;
	}
#ifdef _WIN32
	pName = wutf8_widen(RING_API_GETSTRING(1));
	if (pName == NULL) {
		RING_API_ERROR(WUTF8_ERROR_OOM);
		return;
	}
	/* Repeat if the value changed (by another thread) and the buffer became small */
	nSize = GetEnvironmentVariableW(pName, NULL, 0);
	while (nSize > 0) {
		pValue = (wchar_t *)malloc(sizeof(wchar_t) * (nSize + 1));
		if (pValue == NULL) {
			free(pName);
			RING_API_ERROR(WUTF8_ERROR_OOM);
			return;
		}
		nLen = GetEnvironmentVariableW(pName, pValue, nSize + 1);
		if (nLen <= nSize) {
			/* nLen is 0 if the variable is removed now */
			pValue[nLen] = L'\0';
			wutf8_retwide(pPointer, pValue);
			free(pValue);
			free(pName);
			return;
		}
		free(pValue);
		nSize = nLen;
	}
	free(pName);
	RING_API_RETSTRING(RING_CSTR_EMPTY);
#else
	cValue = getenv(RING_API_GETSTRING(1));
	RING_API_RETSTRING((cValue != NULL) ? cValue : RING_CSTR_EMPTY);
#endif
}

/*
**  sysset(cVariable,cValue) --> 1 (Success) or 0
**  On MS-Windows the process environment is used (w_sysget(), w_system() and child processes see it)
**  Like the Ring function on MS-Windows, an empty value removes the variable
*/
RING_FUNC(ring_w_sysset) {
#ifdef _WIN32
	wchar_t *pName, *pValue;
	int lResult;
#endif
	if (!wutf8_checkstrings(pPointer, 2)) {
		return;
	}
#ifdef _WIN32
	pName = wutf8_widen(RING_API_GETSTRING(1));
	pValue = wutf8_widen(RING_API_GETSTRING(2));
	if ((pName == NULL) || (pValue == NULL)) {
		free(pName);
		free(pValue);
		RING_API_ERROR(WUTF8_ERROR_OOM);
		return;
	}
	if (pValue[0] == L'\0') {
		lResult = SetEnvironmentVariableW(pName, NULL) || (GetLastError() == ERROR_ENVVAR_NOT_FOUND);
	} else {
		lResult = SetEnvironmentVariableW(pName, pValue) != 0;
	}
	RING_API_RETNUMBER(lResult ? 1 : 0);
	free(pName);
	free(pValue);
#else
	RING_API_RETNUMBER(setenv(RING_API_GETSTRING(1), RING_API_GETSTRING(2), 1) == 0 ? 1 : 0);
#endif
}

/* sysunset(cVariable) --> 1 (Success) or 0 */
RING_FUNC(ring_w_sysunset) {
#ifdef _WIN32
	wchar_t *pName;
	int lResult;
#endif
	if (!wutf8_checkstrings(pPointer, 1)) {
		return;
	}
#ifdef _WIN32
	pName = wutf8_widen(RING_API_GETSTRING(1));
	if (pName == NULL) {
		RING_API_ERROR(WUTF8_ERROR_OOM);
		return;
	}
	lResult = SetEnvironmentVariableW(pName, NULL) || (GetLastError() == ERROR_ENVVAR_NOT_FOUND);
	RING_API_RETNUMBER(lResult ? 1 : 0);
	free(pName);
#else
	RING_API_RETNUMBER(unsetenv(RING_API_GETSTRING(1)) == 0 ? 1 : 0);
#endif
}

/*
**  makedir(cFolder) - Like MakeDir() in the Ring standard library
**  Creates the folder and the missing parent folders (like mkdir -p), nothing is returned
**  An existing folder is not an error
*/
RING_FUNC(ring_w_makedir) {
	size_t x, nLen;
#ifdef _WIN32
	wchar_t *pPath;
	int lResult;
#else
	char *cPath;
#endif
	if (!wutf8_checkstrings(pPointer, 1)) {
		return;
	}
#ifdef _WIN32
	/*
	**  Each folder is created using its own path (like the Win32 functions do for a short path)
	**  If we create the parents using one long \\?\ path, spaces and dots at the end of the names are kept
	*/
	pPath = wutf8_widen(RING_API_GETSTRING(1));
	if (pPath == NULL) {
		RING_API_ERROR(WUTF8_ERROR_OOM);
		return;
	}
	nLen = wcslen(pPath);
	for (x = 0; x < nLen; x++) {
		if (pPath[x] == L'/') {
			pPath[x] = L'\\';
		}
	}
	/*
	**  Remove the separators at the end ("a\b\" is "a\b")
	**  Else Windows would keep the spaces/dots at the end of the last name ("a\b \" creates "b ")
	*/
	while ((nLen > 1) && (pPath[nLen - 1] == L'\\')) {
		pPath[--nLen] = L'\0';
	}
	/* Create the parent folders (Errors like "C:" or "\\server" are ignored) */
	for (x = 1; x < nLen; x++) {
		if ((pPath[x] == L'\\') && (pPath[x - 1] != L'\\') && (pPath[x - 1] != L'?')) {
			pPath[x] = L'\0';
			lResult = wutf8_createfolder(pPath);
			pPath[x] = L'\\';
			if (!lResult) {
				free(pPath);
				RING_API_ERROR(WUTF8_ERROR_OOM);
				return;
			}
		}
	}
	lResult = wutf8_createfolder(pPath);
	free(pPath);
	if (!lResult) {
		RING_API_ERROR(WUTF8_ERROR_OOM);
	}
#else
	nLen = strlen(RING_API_GETSTRING(1));
	cPath = (char *)malloc(nLen + 1);
	if (cPath == NULL) {
		RING_API_ERROR(WUTF8_ERROR_OOM);
		return;
	}
	memcpy(cPath, RING_API_GETSTRING(1), nLen + 1);
	for (x = 1; x < nLen; x++) {
		if ((cPath[x] == '/') && (cPath[x - 1] != '/')) {
			cPath[x] = '\0';
			mkdir(cPath, 0777);
			cPath[x] = '/';
		}
	}
	if (nLen > 0) {
		mkdir(cPath, 0777);
	}
	free(cPath);
#endif
}

/* removedir(cFolder) --> 1 (Success) or 0 (The folder must be empty) */
RING_FUNC(ring_w_removedir) {
#ifdef _WIN32
	wchar_t *pPath;
#endif
	if (!wutf8_checkstrings(pPointer, 1)) {
		return;
	}
#ifdef _WIN32
	pPath = wutf8_getpath(pPointer, 1, NULL);
	if (pPath == NULL) {
		return;
	}
	RING_API_RETNUMBER(RemoveDirectoryW(pPath) ? 1 : 0);
	free(pPath);
#else
	RING_API_RETNUMBER(rmdir(RING_API_GETSTRING(1)) == 0 ? 1 : 0);
#endif
}

/* copyfile(cSource,cDestination) --> 1 (Success) or 0 (The destination is replaced if it exists) */
RING_FUNC(ring_w_copyfile) {
#ifdef _WIN32
	wchar_t *pSource, *pDest;
#else
	FILE *pIn, *pOut;
	char *cBuffer;
	size_t nCount;
	int lResult;
	struct stat oSource, oDest;
#endif
	if (!wutf8_checkstrings(pPointer, 2)) {
		return;
	}
#ifdef _WIN32
	pSource = wutf8_getpath(pPointer, 1, NULL);
	if (pSource == NULL) {
		return;
	}
	pDest = wutf8_getpath(pPointer, 2, NULL);
	if (pDest == NULL) {
		free(pSource);
		return;
	}
	RING_API_RETNUMBER(CopyFileW(pSource, pDest, FALSE) ? 1 : 0);
	free(pSource);
	free(pDest);
#else
	/* The source must be a regular file (Like CopyFileW(), the destination is not changed if it's not) */
	if ((stat(RING_API_GETSTRING(1), &oSource) != 0) || !S_ISREG(oSource.st_mode)) {
		RING_API_RETNUMBER(0);
		return;
	}
	/* The same file (maybe using another path), opening the destination would delete the data */
	if ((stat(RING_API_GETSTRING(2), &oDest) == 0) && (oSource.st_dev == oDest.st_dev) &&
	    (oSource.st_ino == oDest.st_ino)) {
		RING_API_RETNUMBER(0);
		return;
	}
	cBuffer = (char *)malloc(WUTF8_COPYBUFFER);
	if (cBuffer == NULL) {
		RING_API_ERROR(WUTF8_ERROR_OOM);
		return;
	}
	lResult = 0;
	pIn = fopen(RING_API_GETSTRING(1), "rb");
	if (pIn != NULL) {
		pOut = fopen(RING_API_GETSTRING(2), "wb");
		if (pOut != NULL) {
			lResult = 1;
			while ((nCount = fread(cBuffer, 1, WUTF8_COPYBUFFER, pIn)) > 0) {
				if (fwrite(cBuffer, 1, nCount, pOut) != nCount) {
					lResult = 0;
					break;
				}
			}
			if (ferror(pIn)) {
				lResult = 0;
			}
			if (fclose(pOut) != 0) {
				lResult = 0;
			}
		}
		fclose(pIn);
	}
	free(cBuffer);
	RING_API_RETNUMBER(lResult);
#endif
}

#ifdef _WIN32
/*
**  Check that the wide argument is the same as the ANSI argument (from the C runtime) of the Ring VM
**  The C runtime and CommandLineToArgvW() have small differences in parsing quotes
**  So we compare the ANSI version of the wide argument with the ANSI argument
*/
static int wutf8_sameargument(const wchar_t *pWide, const char *cAnsi) {
	int nLen, lSame;
	char *cStr;
	nLen = WideCharToMultiByte(CP_ACP, 0, pWide, -1, NULL, 0, NULL, NULL);
	if (nLen <= 0) {
		return RING_FALSE;
	}
	cStr = (char *)malloc(nLen);
	if (cStr == NULL) {
		return RING_FALSE;
	}
	if (WideCharToMultiByte(CP_ACP, 0, pWide, -1, cStr, nLen, NULL, NULL) != nLen) {
		free(cStr);
		return RING_FALSE;
	}
	lSame = strcmp(cStr, cAnsi) == 0;
	free(cStr);
	return lSame;
}
#endif

/*
**  sysargv() --> List of the command line arguments (UTF-8)
**  On MS-Windows the Ring VM gets ANSI arguments (non-ANSI characters become ?)
**  So we parse the wide command line, each argument that matches the Ring VM argument is used
**  Other arguments (and all of them if the count is different) come from the Ring VM list
*/
RING_FUNC(ring_w_sysargv) {
	List *pList;
	RingState *pRingState;
	int x;
#ifdef _WIN32
	wchar_t **pArgs;
	int nArgs;
	char *cArg;
#endif
	if (RING_API_PARACOUNT != 0) {
		RING_API_ERROR(RING_API_BADPARACOUNT);
		return;
	}
	pRingState = ((VM *)pPointer)->pRingState;
	pList = RING_API_NEWLIST;
#ifdef _WIN32
	pArgs = CommandLineToArgvW(GetCommandLineW(), &nArgs);
	if (pArgs != NULL) {
		if (nArgs == pRingState->nArgc) {
			for (x = 0; x < nArgs; x++) {
				cArg = NULL;
				if (wutf8_sameargument(pArgs[x], pRingState->pArgv[x])) {
					cArg = wutf8_narrow(pArgs[x]);
				}
				if (cArg == NULL) {
					ring_list_addstring_gc(pRingState, pList, pRingState->pArgv[x]);
				} else {
					ring_list_addstring_gc(pRingState, pList, cArg);
					free(cArg);
				}
			}
			LocalFree(pArgs);
			RING_API_RETLISTBYREF(pList);
			return;
		}
		LocalFree(pArgs);
	}
#endif
	for (x = 0; x < pRingState->nArgc; x++) {
		ring_list_addstring_gc(pRingState, pList, pRingState->pArgv[x]);
	}
	RING_API_RETLISTBYREF(pList);
}

/* iswidepathapi() --> 1 if the functions use the wide character Windows API */
RING_FUNC(ring_w_iswidepathapi) {
#ifdef _WIN32
	RING_API_RETNUMBER(1);
#else
	RING_API_RETNUMBER(0);
#endif
}

/* Register the functions */

RING_LIBINIT
{
	RING_API_REGISTER("w_read",ring_w_read);
	RING_API_REGISTER("w_write",ring_w_write);
	RING_API_REGISTER("w_fexists",ring_w_fexists);
	RING_API_REGISTER("w_direxists",ring_w_direxists);
	RING_API_REGISTER("w_getpathtype",ring_w_getpathtype);
	RING_API_REGISTER("w_getfilesize",ring_w_getfilesize);
	RING_API_REGISTER("w_dir",ring_w_dir);
	RING_API_REGISTER("w_rename",ring_w_rename);
	RING_API_REGISTER("w_remove",ring_w_remove);
	RING_API_REGISTER("w_tempname",ring_w_tempname);
	RING_API_REGISTER("w_chdir",ring_w_chdir);
	RING_API_REGISTER("w_currentdir",ring_w_currentdir);
	RING_API_REGISTER("w_exefilename",ring_w_exefilename);
	RING_API_REGISTER("w_exefolder",ring_w_exefolder);
	RING_API_REGISTER("w_system",ring_w_system);
	RING_API_REGISTER("w_sysget",ring_w_sysget);
	RING_API_REGISTER("w_sysset",ring_w_sysset);
	RING_API_REGISTER("w_sysunset",ring_w_sysunset);
	RING_API_REGISTER("w_makedir",ring_w_makedir);
	RING_API_REGISTER("w_removedir",ring_w_removedir);
	RING_API_REGISTER("w_copyfile",ring_w_copyfile);
	RING_API_REGISTER("w_sysargv",ring_w_sysargv);
	RING_API_REGISTER("w_iswidepathapi",ring_w_iswidepathapi);
}
