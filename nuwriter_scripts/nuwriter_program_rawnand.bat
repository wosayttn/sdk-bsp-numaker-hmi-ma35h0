py -3 nuwriter.py -c header-nand.json
py -3 nuwriter.py -p pack-nand.json

py -3 nuwriter.py -a ddrimg\MA35H04F767C.bin
IF %ERRORLEVEL% EQU 0 (
    py -3 nuwriter.py -w nand pack/pack.bin
)
