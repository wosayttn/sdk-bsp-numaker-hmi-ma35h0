@echo off

set "filename=%~n0"
echo Batch file name is %filename%

echo bin\ffplay -protocol_whitelist "file,rtp,udp" "%filename%.sdp" > "play_%filename%.bat"

bin\ffmpeg -f dshow -i video="c922 Pro Stream Webcam" ^
    -vf scale=1280:720 ^
    -vcodec libx264 -g 30 -preset ultrafast -tune zerolatency ^
    -pix_fmt yuv420p -f rtp ^
    -protocol_whitelist "file,udp,rtp,pipe" ^
    -sdp_file "%filename%.sdp" ^
    "rtp://192.168.0.255:5004?pkt_size=1400"

pause