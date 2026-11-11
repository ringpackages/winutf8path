# WinUTF8Path Package

## Overview

WinUTF8Path package for the Ring programming language

Ring strings are UTF-8. On MS-Windows, some file functions in the Ring VM pass the path to ANSI C functions (`stat()`, `rename()`, `remove()`, `_chdir()`) or to the ANSI Windows API (`FindFirstFileA()`). These functions read the path in the current ANSI code page, so a path with characters outside that code page (Cyrillic, Arabic, Chinese, Greek, emoji, ...) is corrupted and the function fails:

```ring
fexists("Папка/Тестовый файл Ёжик.txt")      # 1  (works)
getfilesize("Папка/Тестовый файл Ёжик.txt")  # -1 (fails)
direxists("Папка")                           # 0  (fails)
dir("Папка")                                 # Error (fails)
```

This package provides the same functions with the `w_` prefix: `w_dir()`, `w_rename()`, `w_remove()`, `w_getfilesize()`, ... On MS-Windows they convert the path from UTF-8 to UTF-16, call the wide character Windows API (`...W` functions), and return file names and folders as UTF-8. On Linux and macOS, where file paths are already UTF-8, they use the ANSI C/POSIX functions like the Ring functions do.

The library is simple and self-contained:

- It doesn't replace or change the Ring functions. Use the `w_*()` functions where you need UTF-8 paths.
- It doesn't call the file/OS functions of the Ring VM. All the code is in the extension.

The package is one folder, `ring/extensions/winutf8path`:

- **C Extension**: one C file (`ring_winutf8path.c`), ANSI C code that uses the Windows API. It builds to `ring_winutf8path.dll`, `libring_winutf8path.so` or `libring_winutf8path.dylib`.
- **Ring Code**: `winutf8path.ring` loads the extension and the Ring code (`globals.ring`, `functions.ring`, which provides `w_version()`). Programs use `load "winutf8path.ring"` (through `ring/bin/load/winutf8path.ring`).

## Features

- UTF-8 paths in `w_dir()`, `w_rename()`, `w_remove()`, `w_getfilesize()`, `w_getpathtype()`, `w_direxists()`, `w_chdir()`, `w_currentdir()`, ... on MS-Windows
- `w_dir()` returns the real file names as UTF-8 (not `?` or corrupted characters)
- `w_makedir()` creates a folder and its missing parents (like `mkdir -p`)
- `w_system()`, `w_sysget()`, `w_sysset()` and `w_sysunset()` with UTF-8 commands, names and values
- `w_sysargv()` returns the command line arguments as UTF-8 (`sysargv` has `?` for non-ANSI characters on MS-Windows)
- Long paths: when the full path has 248 characters or more, the `\\?\` prefix is added automatically, so paths longer than 260 characters work (except in `w_chdir()`)
- Strings that are not valid UTF-8 are read in the ANSI code page (like the Ring VM), so old code keeps working
- Paths that contain `char(0)` raise an error (the C functions would stop at it and use another path)
- The same parameters, return values and error messages as the Ring functions, so `dir()` becomes `w_dir()` without other changes (see [Differences from the Ring Functions](#differences-from-the-ring-functions))
- Extra functions: `w_removedir()`, `w_copyfile()`, `w_sysargv()`
- Tested on MS-Windows and Linux, including memory checks (a checking allocator, AddressSanitizer and UBSan) and fuzzing. The macOS and FreeBSD code compiles without warnings.

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

### Error Messages

| Error | When |
|-------|------|
| `Error, WinUTF8Path: The string contains the NUL character` | A path, a command or an environment variable name/value contains `char(0)` |
| `Error, Couldn't open the directory` | `w_dir()` can't open the folder (same as `dir()`) |
| `Error (R35) : Can't create/open the file` | `w_read()` can't open the file (on MS-Windows also a folder), or `w_write()` can't create it (same as `read()`/`write()`) |
| `Error (E14) : Can't read file` | `w_read()` can't read the file: a file of 2 GB or more, or a folder on Linux and macOS (same as `read()`) |
| `Error (R39) : Error occurred creating unique filename.` | `w_tempname()` fails (same as `tempname()`) |

Like other Ring errors, these errors can be caught using `try/catch`.

## Differences from the Ring Functions

These functions differ from the Ring functions (and between platforms) on purpose:

| Function | Difference |
|----------|------------|
| All functions | A path that contains `char(0)` raises an error. The Ring functions use the part before `char(0)` (for example `remove("a.txt" + char(0) + "x")` deletes `a.txt`). |
| `w_rename()` | On MS-Windows, the new path must not exist (like `rename()` on MS-Windows); on Linux and macOS an existing file is replaced. On MS-Windows, a new path that ends with `/` or `\` works only if the old path is a folder (like Linux). |
| `w_tempname()` | On Linux and macOS the empty file is created (so no other program can get the same name). On MS-Windows only the name is returned. |
| `w_system()` | On MS-Windows the result is the exit code of the command. On Linux and macOS it's the wait status (`exit 3` returns `768`, that is 3 × 256), like `system()`. |
| `w_sysset()` | On MS-Windows an empty value removes the variable (like `sysset()` on MS-Windows); on Linux and macOS the variable is set to `""`. |
| `w_sysset()`, `w_sysget()` | On MS-Windows they use the process environment. `w_sysget()`, `w_system()` and child processes see the values set by `w_sysset()`, but the Ring `sysget()` function keeps its own copy of the environment. Don't mix `w_sysset()` with `sysget()` (or `sysset()` with `w_sysget()`). |
| `w_dir()` | On MS-Windows, the root of an empty drive returns `[]` (the Ring function raises an error). |
| `w_copyfile()` | On MS-Windows the attributes and the dates are copied too (`CopyFileW()`). On Linux and macOS only the content is copied. |

`fopen()` already supports UTF-8 paths on MS-Windows (the Ring VM uses `_wfopen()`), so the package has no `w_fopen()`. The `FILE` pointer must be created by the C runtime of the Ring VM, because `fgets()`, `fclose()`, ... use it. `fopen()` doesn't support paths longer than 260 characters; use `w_read()`/`w_write()` for long paths.

## Path Rules on MS-Windows

The functions follow the Win32 path rules (like the Ring functions and other Windows programs):

- `/` and `\` can both be used as the separator.
- Spaces and dots at the end of the last name are removed: `"Файл.txt "` is `"Файл.txt"`. A file whose name really ends with a space or a dot (created by another program) can be used with a `\\?\` path, for example `"\\?\C:\Папка\Файл "`.
- Paths that start with `\\?\` are used as they are: use full paths and `\` only.
- `:` after a name is the NTFS stream syntax: `w_write("Файл.txt:info", "x")` writes the stream `info` of `Файл.txt` (not a new file).
- `*` and `?` are not allowed in names.
- If a path is not valid UTF-8, the whole path is read in the ANSI code page. Names returned by `w_dir()`, `w_currentdir()`, ... are always UTF-8.
- `w_makedir("a/b/")` and `w_makedir("a/b")` are the same.
- `w_chdir()` can't use folders longer than 260 characters (a Windows limit).

To see UTF-8 text in the Windows console, use `chcp 65001` or Windows Terminal.

## Using w_dir() in a for loop

Ring evaluates the expression after `in` in each iteration of a `for ... in` loop. So store the result of `w_dir()` in a variable first. It's faster, and the loop sees all the files even if the loop deletes some of them:

```ring
aList = w_dir(cFolder)
for aItem in aList
	? aItem[1]
next
```

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

Output of `test02_compare_functions.ring` on MS-Windows:

```
Function       Original   w_*()
--------------------------------
fexists        1          1
direxists      0          1
getfilesize    -1         5
getpathtype    0          1
dir            Error      1
```

On Linux and macOS both columns are the same.

## Known Limitations

- Error messages that show the path of the main source file come from the Ring VM, which builds that path with ANSI functions, so its Cyrillic part is still corrupted. Files loaded using `load` are not affected.
- `w_read()` reads files smaller than 2 GB (like `read()`).

## Building the Extension

Go to `ring/extensions/winutf8path` and run:

| Platform | Command |
|----------|---------|
| MS-Windows (32-bit) | `buildvc.bat` |
| MS-Windows (64-bit) | `buildvc_x64.bat` |
| Linux | `./buildgcc.sh` |
| macOS | `./buildclang.sh` |

The MS-Windows build writes `ring/bin/ring_winutf8path.dll`; the Linux and macOS builds write `libring_winutf8path.so` and `libring_winutf8path.dylib` to `ring/lib`. On other Unix systems (like FreeBSD), use the commands of `buildgcc.sh` with the system C compiler.

The extension is one C file (`ring_winutf8path.c`), so no code generation step is needed.

To distribute a program using `ring2exe`, add the `-winutf8path` option to copy the extension library.

## License

MIT License
