:forever_develop
py -3 nuwriter.py -a ddrimg\MA35H04F767C.bin
IF %ERRORLEVEL% EQU 0 (
   py -3 nuwriter.py -o execute -w ddr 0x80800000 ..\rtthread.bin
)
pause

goto :forever_develop
