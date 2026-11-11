# Folders: w_makedir() creates the parent folders too, w_chdir(), w_currentdir(), w_removedir()

load "winutf8path.ring"

func main
	cStart = w_currentdir()
	cRoot  = cStart + "/Корень"
	cDeep  = cRoot + "/Уровень 1/المستوى 2/第三级"

	w_makedir(cDeep)
	? "Created     : " + w_direxists(cDeep)
	? "w_chdir()   : " + w_chdir(cDeep)
	? "Current     : " + w_currentdir()
	w_write("файл.txt", "data")
	? "Files here  : " + len(w_dir("."))
	w_chdir(cStart)

	# Remove the tree (w_removedir() needs an empty folder)
	w_remove(cDeep + "/файл.txt")
	cPath = cDeep
	while len(cPath) > len(cRoot)
		w_removedir(cPath)
		cPath = left(cPath, len(cPath) - len(lastPart(cPath)) - 1)
	end
	? "w_removedir(): " + w_removedir(cRoot)
	? "Exists       : " + w_direxists(cRoot)

func lastPart cPath
	for x = len(cPath) to 1 step -1
		if cPath[x] = "/"
			return substr(cPath, x + 1)
		ok
	next
	return cPath
