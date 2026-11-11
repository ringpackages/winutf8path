# Unit tests for the w_*() functions
# Runs on all platforms (MS-Windows, Linux, macOS)

load "winutf8path.ring"

nPassed = 0
nFailed = 0

func main
	aNames = [
		"Тестовый файл Ёжик",
		"ملف عربي",
		"中文文件",
		"Ελληνικά",
		"Emoji 😀",
		"ASCII name"
	]
	cBase = w_currentdir() + "/winutf8path_tests"
	w_makedir(cBase)
	check("base folder", w_direxists(cBase), 1)

	? "== w_*() functions"
	for cName in aNames
		testName(cBase, cName)
	next

	? "== Other functions"
	cTemp = w_tempname()
	check("tempname not empty", len(cTemp) > 0, 1)
	w_remove(cTemp)
	check("makedir existing folder", w_direxists(cBase), 1)
	w_makedir(cBase)
	check("makedir again", w_direxists(cBase), 1)
	check("read missing raises error", raisesError(:w_read, cBase + "/Нет"), 1)
	check("write bad folder raises error", raisesError(:w_write, cBase + "/Нет/файл.txt"), 1)
	check("bad parameter type raises error", raisesError(:w_fexists, 1), 1)
	check("w_version", w_version(), WINUTF8PATH_VERSION)
	check("exefilename", w_fexists(w_exefilename()), 1)
	check("exefolder", w_direxists(w_exefolder()), 1)
	check("sysargv count", len(w_sysargv()), len(sysargv))
	check("sysset", w_sysset("WINUTF8_TEST", "Ёжик ملف"), 1)
	check("sysget", w_sysget("WINUTF8_TEST"), "Ёжик ملف")
	check("sysset number", w_sysset("WINUTF8_NUMBER", "7"), 1)
	if isWindows()
		check("system sees sysset", w_system("exit /b %WINUTF8_NUMBER%"), 7)
	else
		check("system sees sysset", w_system("exit $WINUTF8_NUMBER") / 256, 7)
	ok
	check("sysunset number", w_sysunset("WINUTF8_NUMBER"), 1)
	check("sysunset", w_sysunset("WINUTF8_TEST"), 1)
	check("sysget removed", w_sysget("WINUTF8_TEST"), "")
	check("getpathtype not found", w_getpathtype(cBase + "/Нет"), 0)
	check("getfilesize folder", w_getfilesize(cBase), -1)
	check("rename missing", w_rename(cBase + "/Нет", cBase + "/Нет2"), -1)
	check("chdir missing", w_chdir(cBase + "/Нет"), -1)
	check("dir missing raises error", raisesError(:w_dir, cBase + "/Нет"), 1)

	? "== Edge cases"
	testEdgeCases(cBase)

	check("removedir", w_removedir(cBase), 1)
	? ""
	? "Passed: " + nPassed + "  Failed: " + nFailed

func testName cBase, cName
	cFolder = cBase + "/" + cName
	cFile   = cFolder + "/" + cName + ".txt"
	cFile2  = cFolder + "/" + cName + " 2.txt"
	cCopy   = cFolder + "/" + cName + " copy.txt"
	cData   = "Data: " + cName
	cStart  = w_currentdir()
	w_makedir(cFolder)
	check(cName + " direxists", w_direxists(cFolder), 1)
	w_write(cFile, cData)
	check(cName + " fexists", w_fexists(cFile), 1)
	check(cName + " fexists (folder)", w_fexists(cFolder), 0)
	check(cName + " direxists (file)", w_direxists(cFile), 0)
	check(cName + " read", w_read(cFile), cData)
	check(cName + " getfilesize", w_getfilesize(cFile), len(cData))
	check(cName + " getpathtype file", w_getpathtype(cFile), 1)
	check(cName + " getpathtype dir", w_getpathtype(cFolder), 2)
	aList = w_dir(cFolder)
	check(cName + " dir", len(aList) = 1 and aList[1][1] = cName + ".txt" and aList[1][2] = 0, 1)
	check(cName + " copyfile", w_copyfile(cFile, cCopy), 1)
	check(cName + " copy content", w_read(cCopy), cData)
	w_remove(cCopy)
	check(cName + " chdir", w_chdir(cFolder), 0)
	check(cName + " currentdir", substr(w_currentdir(), cName) > 0, 1)
	check(cName + " fexists (relative)", w_fexists(cName + ".txt"), 1)
	w_chdir(cStart)
	check(cName + " rename", w_rename(cFile, cFile2), 0)
	check(cName + " renamed", w_fexists(cFile2) and ! w_fexists(cFile), 1)
	w_remove(cFile2)
	check(cName + " remove", w_fexists(cFile2), 0)
	check(cName + " removedir not empty", w_removedir(cBase), 0)
	check(cName + " removedir", w_removedir(cFolder), 1)

func testEdgeCases cBase
	cFolder = cBase + "/Края"
	cFile   = cFolder + "/Ёжик.txt"
	cData   = "data" + char(0) + "more"
	w_makedir(cFolder + "/")
	check("makedir with / at the end", w_direxists(cFolder), 1)
	w_write(cFile, cData)
	check("write/read data with NUL", len(w_read(cFile)) = len(cData) and substr(w_read(cFile), "more") = 6, 1)
	check("path with NUL raises error", raisesError(:w_fexists, cFile + char(0) + "x"), 1)
	check("dir with / at the end", len(w_dir(cFolder + "/")), 1)
	check("read folder raises error", raisesError(:w_read, cFolder), 1)
	check("copyfile to the same file", w_copyfile(cFile, cFolder + "/./Ёжик.txt"), 0)
	check("copyfile kept the data", w_getfilesize(cFile), len(cData))
	check("copyfile folder source", w_copyfile(cFolder, cFolder + "/copy.txt"), 0)
	check("copyfile folder source: no file", w_fexists(cFolder + "/copy.txt"), 0)
	check("rename file to path/", w_rename(cFile, cFolder + "/Новый/"), -1)
	check("rename file to path/: kept", w_fexists(cFile), 1)
	w_makedir(cFolder + "/Папка")
	check("rename folder to path/", w_rename(cFolder + "/Папка", cFolder + "/Папка2/"), 0)
	check("rename folder to path/: done", w_direxists(cFolder + "/Папка2"), 1)
	w_removedir(cFolder + "/Папка2")
	w_remove(cFile)
	check("edge cases removedir", w_removedir(cFolder), 1)

func check cTest, vValue, vExpected
	if vValue = vExpected
		nPassed++
	else
		nFailed++
		? "FAILED: " + cTest + " -> [" + vValue + "] expected [" + vExpected + "]"
	ok

# Returns 1 if calling the function (one parameter) raises an error
func raisesError cFunc, vPara
	try
		if cFunc = :w_read
			w_read(vPara)
		but cFunc = :w_write
			w_write(vPara, "data")
		but cFunc = :w_fexists
			w_fexists(vPara)
		but cFunc = :w_dir
			w_dir(vPara)
		ok
		return 0
	catch
		return 1
	done
