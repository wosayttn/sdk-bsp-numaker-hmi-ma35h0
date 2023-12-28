py -3 nuwriter.py -c header-sd.json
py -3 nuwriter.py -p pack-sd.json

py -3 nuwriter.py -a ddrimg\MA35H04F767C.bin
IF %ERRORLEVEL% EQU 0 (
    py -3 nuwriter.py -w sd pack/pack.bin
)
PAUSE
