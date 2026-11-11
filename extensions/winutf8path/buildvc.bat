cls
setlocal enableextensions enabledelayedexpansion
call ../../language/build/locatevc.bat auto %1
cl /c %ringcflags% ring_winutf8path.c -I"..\..\language\include"
link %ringldflags% ring_winutf8path.obj ..\..\lib\ring.lib shell32.lib /DLL /OUT:..\..\bin\ring_winutf8path.dll /SUBSYSTEM:CONSOLE,"5.01" 
del *.obj
endlocal
