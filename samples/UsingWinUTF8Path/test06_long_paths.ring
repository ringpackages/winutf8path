# Long paths (more than 260 characters)
# The w_*() functions add the \?\ prefix automatically on MS-Windows

load "winutf8path.ring"

func main
	cRoot = w_currentdir() + "/Длинный путь"
	cPath = cRoot
	for x = 1 to 12
		cPath += "/Папка номер " + x
	next
	? "Path length (characters) : " + charCount(cPath)

	w_makedir(cPath)
	? "w_direxists()   : " + w_direxists(cPath)
	cFile = cPath + "/Файл.txt"
	w_write(cFile, "Long path")
	? "w_read()        : " + w_read(cFile)
	? "w_getfilesize() : " + w_getfilesize(cFile)

	# Clean up
	w_remove(cFile)
	while len(cPath) >= len(cRoot)
		w_removedir(cPath)
		nPos = 0
		for x = len(cPath) to 1 step -1
			if cPath[x] = "/" nPos = x exit ok
		next
		cPath = left(cPath, nPos - 1)
	end
	? "Removed         : " + (w_direxists(cRoot) = 0)

# The count of characters in a UTF-8 string (not bytes)
func charCount cStr
	nCount = 0
	for x = 1 to len(cStr)
		if (ascii(cStr[x]) & 192) != 128
			nCount++
		ok
	next
	return nCount
