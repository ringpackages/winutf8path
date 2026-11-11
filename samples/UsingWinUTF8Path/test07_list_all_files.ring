# List all the files in a folder and its sub folders using w_dir()
# Note: Ring evaluates the expression after "in" in each iteration of a for-in loop
# So we store the result of w_dir() (and of the recursive calls) in a variable first

load "winutf8path.ring"

func main
	cRoot = "Документы"
	w_makedir(cRoot + "/Отчёты/2026")
	w_makedir(cRoot + "/تقارير")
	w_write(cRoot + "/Записка.txt", "1")
	w_write(cRoot + "/Отчёты/Отчёт.txt", "22")
	w_write(cRoot + "/Отчёты/2026/Итоги.txt", "333")
	w_write(cRoot + "/تقارير/ملف.txt", "4444")

	? "Files:"
	aFiles = listAllFiles(cRoot)
	for cFile in aFiles
		? "	" + cFile + " (" + w_getfilesize(cFile) + " bytes)"
	next

	# Delete the files, then the folders (deepest first)
	for cFile in aFiles
		w_remove(cFile)
	next
	aFolders = listAllFolders(cRoot)
	for x = len(aFolders) to 1 step -1
		w_removedir(aFolders[x])
	next
	w_removedir(cRoot)
	? "Removed: " + (w_direxists(cRoot) = 0)

func listAllFiles cFolder
	aOutput = []
	aList = w_dir(cFolder)
	for aItem in aList
		cPath = cFolder + "/" + aItem[1]
		if aItem[2]
			aSubFiles = listAllFiles(cPath)
			for cFile in aSubFiles
				aOutput + cFile
			next
		else
			aOutput + cPath
		ok
	next
	return aOutput

func listAllFolders cFolder
	aOutput = []
	aList = w_dir(cFolder)
	for aItem in aList
		if aItem[2]
			cPath = cFolder + "/" + aItem[1]
			aOutput + cPath
			aSubFolders = listAllFolders(cPath)
			for cSub in aSubFolders
				aOutput + cSub
			next
		ok
	next
	return aOutput
