# Using the w_*() functions directly (They work on all platforms)

load "winutf8path.ring"

func main
	cFolder = currentdir() + "/Обучение"
	cFile   = cFolder + "/Тестовый файл Ёжик.txt"
	cNew    = cFolder + "/Ёжик.txt"

	w_makedir(cFolder)
	? "w_direxists()   : " + w_direxists(cFolder)

	w_write(cFile, "Модуль загружен")
	? "w_fexists()     : " + w_fexists(cFile)
	? "w_read()        : " + w_read(cFile)
	? "w_getfilesize() : " + w_getfilesize(cFile)
	? "w_getpathtype() : " + w_getpathtype(cFile) + " (1 = File)"

	? "w_dir()         :"
	aList = w_dir(cFolder)
	for aItem in aList
		? "	" + aItem[1] + " (Folder: " + aItem[2] + ")"
	next

	? "w_rename()      : " + w_rename(cFile, cNew) + " (0 = Success)"
	? "w_fexists(new)  : " + w_fexists(cNew)

	w_remove(cNew)
	? "After w_remove(): " + w_fexists(cNew)
	? "w_removedir()   : " + w_removedir(cFolder)
