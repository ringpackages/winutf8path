# WinUTF8Path Package

## Overview

WinUTF8Path package for the Ring programming language

This package provides the same file processing functions with the `w_` prefix: `w_dir()`, `w_rename()`, `w_remove()`, `w_getfilesize()`, ... On MS-Windows they convert the path from UTF-8 to UTF-16, call the wide character Windows API (`...W` functions), and return file names and folders as UTF-8. On Linux and macOS, where file paths are already UTF-8, they use the ANSI C/POSIX functions like the Ring functions do.

## Features

- UTF-8 paths in `w_dir()`, `w_rename()`, `w_remove()`, `w_getfilesize()`, `w_getpathtype()`, `w_direxists()`, `w_chdir()`, `w_currentdir()`, ... on MS-Windows
- `w_dir()` returns the real file names as UTF-8 (not `?` or corrupted characters)
- `w_makedir()` creates a folder and its missing parents (like `mkdir -p`)
- `w_system()`, `w_sysget()`, `w_sysset()` and `w_sysunset()` with UTF-8 commands, names and values
- `w_sysargv()` returns the command line arguments as UTF-8 (`sysargv` has `?` for non-ANSI characters on MS-Windows)
- Long paths: when the full path has 248 characters or more, the `\\?\` prefix is added automatically, so paths longer than 260 characters work (except in `w_chdir()`)
- Strings that are not valid UTF-8 are read in the ANSI code page (like the Ring VM), so old code keeps working
- Paths that contain `char(0)` raise an error (the C functions would stop at it and use another path)
- The same parameters, return values and error messages as the Ring functions, so `dir()` becomes `w_dir()` without other changes
- Extra functions: `w_removedir()`, `w_copyfile()`, `w_sysargv()`
- Tested on MS-Windows and Linux.

## Installation

	ringpm install winutf8path from ringpackages

The package includes `ring_winutf8path.dll` (64-bit) for MS-Windows. On Linux and macOS, build the extension (see [Building the Extension](#building-the-extension)).

## Quick Start

```ring
load "winutf8path.ring"

w_makedir("Обучение")
w_write("Обучение/Тестовый файл Ёжик.txt", "Модуль загружен")

? w_fexists("Обучение/Тестовый файл Ёжик.txt")       # 1
? w_getfilesize("Обучение/Тестовый файл Ёжик.txt")   # 29
aList = w_dir("Обучение")
for aItem in aList
	? aItem[1]                                         # Тестовый файл Ёжик.txt
next

w_chdir("Обучение")
? w_currentdir()
w_chdir("..")

w_rename("Обучение/Тестовый файл Ёжик.txt", "Обучение/Ёжик.txt")
w_remove("Обучение/Ёжик.txt")
w_removedir("Обучение")
```

## API Reference

### Functions like the Ring functions

Each function has the parameters, return value and error messages of the Ring function with the same name (without `w_`).

| Function | Ring Function | Description |
|----------|---------------|-------------|
| `w_read(cFileName)` | `read()` | Return the file content (files smaller than 2 GB) |
| `w_write(cFileName, cString)` | `write()` | Write the string to the file (creates or replaces it); the string can contain `char(0)` |
| `w_fexists(cFileName)` | `fexists()` | Return `1` if the path is a regular file |
| `w_direxists(cDirPath)` | `direxists()` | Return `1` if the path is a folder |
| `w_getpathtype(cPath)` | `getpathtype()` | Return `0` (not found), `1` (file), `2` (folder) or `-1` (other, like a device) |
| `w_getfilesize(cFilePath)` | `getfilesize()` | Return the file size, or `-1` (not found or not a file) |
| `w_dir(cFolder)` | `dir()` | Return a list of `[cName, lIsFolder]` without `.` and `..`, in the order of the file system (not sorted); raises an error if the folder can't be opened |
| `w_rename(cOldPath, cNewPath)` | `rename()` | Rename or move a file or folder; return `0` on success, `-1` on failure |
| `w_remove(cFileName)` | `remove()` | Delete the file (nothing is returned, like `remove()`) |
| `w_tempname()` | `tempname()` | Return a unique temporary file name |
| `w_chdir(cFolder)` | `chdir()` | Change the current folder; return `0` on success, `-1` on failure |
| `w_currentdir()` | `currentdir()` | Return the current folder |
| `w_exefilename()` | `exefilename()` | Return the executable file name |
| `w_exefolder()` | `exefolder()` | Return the executable folder (ends with the path separator) |
| `w_system(cCommand)` | `system()` | Run the command; return the result of the C `system()` function |
| `w_sysget(cVariable)` | `sysget()` | Return the value of an environment variable (`""` if not found) |
| `w_sysset(cVariable, cValue)` | `sysset()` | Set an environment variable; return `1` on success, `0` on failure |
| `w_sysunset(cVariable)` | `sysunset()` | Remove an environment variable; return `1` on success, `0` on failure |
| `w_makedir(cFolder)` | `MakeDir()` (stdlib) | Create the folder and its missing parents; an existing folder is not an error; nothing is returned (check with `w_direxists()`) |

### Extra Functions

| Function | Description |
|----------|-------------|
| `w_sysargv()` | Return the command line arguments as UTF-8 (the same list as `sysargv`) |
| `w_removedir(cFolder)` | Remove an empty folder; return `1` on success, `0` on failure |
| `w_copyfile(cSource, cDestination)` | Copy a file (the destination is replaced if it exists); return `1` on success, `0` on failure (also when the source is a folder, or the source and the destination are the same file) |
| `w_iswidepathapi()` | Return `1` if the functions use the wide character Windows API (MS-Windows), `0` on other platforms |
| `w_version()` | Return the library version (`"1.0.0"`) |

## Samples

The samples are in `ring/samples/UsingWinUTF8Path`.

| Sample | Description |
|--------|-------------|
| `test01_w_functions.ring` | Using the `w_*()` functions with Cyrillic file and folder names |
| `test02_compare_functions.ring` | The Ring functions vs the `w_*()` functions |
| `test03_folders.ring` | Nested folders, `w_chdir()`, `w_currentdir()`, `w_removedir()` |
| `test04_environment.ring` | Environment variables and `w_system()` |
| `test05_sysargv.ring` | Command line arguments as UTF-8 (run it with arguments, for example `ring test05_sysargv.ring Привет مرحبا`) |
| `test06_long_paths.ring` | Paths with more than 260 characters |
| `test07_list_all_files.ring` | List the files in a folder and its sub folders using `w_dir()` |
| `test08_unit_tests.ring` | Unit tests (Cyrillic, Arabic, Chinese, Greek and emoji names, and edge cases) for all platforms |

On Linux and macOS both columns are the same.

## License

MIT License
