# Command line arguments
# Run: ring test05_sysargv.ring Привет مرحبا
# On MS-Windows sysargv contains ? instead of the non-ANSI characters
# w_sysargv() returns the arguments as UTF-8

load "winutf8path.ring"

func main
	? "sysargv:"
	? sysargv
	? "w_sysargv():"
	? w_sysargv()
