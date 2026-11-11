# Compare the original Ring functions with the w_*() functions
# On MS-Windows the original functions fail with non-ANSI paths (Cyrillic, Arabic, Chinese, ...)
# On Linux/macOS both columns are the same

load "winutf8path.ring"

func main
	cFolder = currentdir() + "/Папка ملفات 文件夹"
	cFile   = cFolder + "/Тестовый файл Ёжик.txt"
	w_makedir(cFolder)
	w_write(cFile, "12345")

	? "Function       Original   w_*()"
	? "--------------------------------"
	show("fexists",     fexists(cFile),     w_fexists(cFile))
	show("direxists",   direxists(cFolder), w_direxists(cFolder))
	show("getfilesize", getfilesize(cFile), w_getfilesize(cFile))
	show("getpathtype", getpathtype(cFile), w_getpathtype(cFile))
	show("dir",         dirCount(cFolder),  len(w_dir(cFolder)))

	w_remove(cFile)
	w_removedir(cFolder)

func show cName, vOriginal, vNew
	? left(cName + copy(" ",15), 15) + left("" + vOriginal + copy(" ",11), 11) + vNew

func dirCount cFolder
	try
		return len(dir(cFolder))
	catch
		return "Error"
	done
