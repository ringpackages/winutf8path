/*
	WinUTF8Path - UTF-8 file paths on MS-Windows for the Ring Programming Language
	Copyright (c) 2026 Mahmoud Fayed <msfclipper@yahoo.com>

	The w_*() functions (written in C) work on all platforms
	On MS-Windows they support UTF-8 paths (Cyrillic, Arabic, Chinese, ...)
	On other platforms they have the same behavior as the Ring functions
*/

# Load the extension (ring_winutf8path.c)
if isWindows()
	LoadLib("ring_winutf8path.dll")
but ismacosx()
	LoadLib("libring_winutf8path.dylib")
else
	LoadLib("libring_winutf8path.so")
ok

load "globals.ring"
load "functions.ring"
