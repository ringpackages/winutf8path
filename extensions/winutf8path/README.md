# WinUTF8Path Extension

## Introduction

This extension provides file functions that support UTF-8 paths on MS-Windows (Cyrillic, Arabic, Chinese, ...).

On MS-Windows each function converts UTF-8 to UTF-16 and calls the wide character Windows API (`FindFirstFileW()`, `GetFileAttributesExW()`, `MoveFileExW()`, `DeleteFileW()`, `CreateDirectoryW()`, `SetCurrentDirectoryW()`, ...), then converts the results back to UTF-8. On Linux, macOS and FreeBSD each function uses the ANSI C/POSIX functions (`stat()`, `opendir()`, `rename()`, `getcwd()`, ...), like the Ring function of the same name. The few intended differences are listed in the package README ("Differences from the Ring Functions").

The extension is self-contained: it uses the Ring extension API only, and doesn't call the file/OS functions of the Ring VM.

Programs load the extension (and the Ring code of the package) using `winutf8path.ring`:

```ring
load "winutf8path.ring"

? w_dir("Папка")
```

## Files

| File | Description |
|------|-------------|
| `ring_winutf8path.c` | The extension source code (ANSI C + Windows API), written by hand: the functions and their registration (`RING_LIBINIT`) |
| `winutf8path.ring` | Loads the extension library, `globals.ring` and `functions.ring` (used by `load "winutf8path.ring"` through `ring/bin/load/winutf8path.ring`) |
| `globals.ring` | Global constants (`WINUTF8PATH_VERSION`) |
| `functions.ring` | Functions written in Ring (`w_version()`) |
| `buildvc.bat`, `buildvc_x64.bat` | Build using Visual C/C++ (32-bit and 64-bit), output: `ring/bin/ring_winutf8path.dll` |
| `buildgcc.sh` | Build using GCC (Linux), output: `ring/lib/libring_winutf8path.so` |
| `buildclang.sh` | Build using Clang (macOS), output: `ring/lib/libring_winutf8path.dylib` |

The extension doesn't use the Ring code generator (like `ringinternet` and `ringmurmurhash`), because the code needs C preprocessor conditions (`#ifdef _WIN32`) for each platform.

To add a function: write it in `ring_winutf8path.c` using `RING_FUNC(ring_w_name)`, register it in `RING_LIBINIT` at the end of the file (`RING_API_REGISTER("w_name", ring_w_name);`), then build.

## Functions

`w_read()`, `w_write()`, `w_fexists()`, `w_direxists()`, `w_getpathtype()`, `w_getfilesize()`, `w_dir()`, `w_rename()`, `w_remove()`, `w_tempname()`, `w_chdir()`, `w_currentdir()`, `w_exefilename()`, `w_exefolder()`, `w_system()`, `w_sysget()`, `w_sysset()`, `w_sysunset()`, `w_makedir()`, `w_removedir()`, `w_copyfile()`, `w_sysargv()`, `w_iswidepathapi()`

See the package README for the description of each function.
