# Environment variables and system() with UTF-8 strings

load "winutf8path.ring"

func main
	? "w_sysset()  : " + w_sysset("WINUTF8_TEST", "Значение قيمة 值")
	? "w_sysget()  : " + w_sysget("WINUTF8_TEST")

	# The command contains a Cyrillic folder name
	cFolder = "Новая папка"
	w_system('mkdir "' + cFolder + '"')
	? "w_system() created the folder : " + w_direxists(cFolder)
	w_removedir(cFolder)

	? "w_sysunset(): " + w_sysunset("WINUTF8_TEST")
	? "w_sysget()  : [" + w_sysget("WINUTF8_TEST") + "]"
