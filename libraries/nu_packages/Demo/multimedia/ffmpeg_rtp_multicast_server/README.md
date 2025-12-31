
## File Descriptions

🛠️ Core Utilities & Setup

- install_ffmpeg.bat: A script that automatically downloads the FFmpeg .zip from GitHub, extracts it to the bin folder, and cleans up temporary files.
  - bin/: Local folder containing FFmpeg, FFplay, and FFprobe executables.
- list_uvc.bat: Use this to list all dshow compatible devices (cameras and microphones) to ensure the device names in your scripts match your hardware.

📡 Streaming Scripts (Transmitters)

These scripts handle hardware capture, encoding (H.264 for video) and UDP packetizing.

|Script Name | Target Address | Features |
| -- | -- | -- |
| broadcast_rtp_720p.bat | 192.168.0.255 | Subnet-wide broadcast. High compatibility for local networks.|
| multicast_rtp_720p.bat | 224.1.1.1 | Efficient multicast streaming. Requires IGMP support on the network.|
| multicast_rtp_360p.bat | 224.1.1.1 | Low-bandwidth version (640x360) for faster processing on MCU/lwIP targets.|

📄 Session Metadata

.sdp Files: Generated automatically when a stream starts. These contain essential metadata (Payload Type, IP, Port) required by receivers like VLC or FFplay to decode the stream.

⚙️ Technical Specifications

- Video Stream (Port 5004)
  - Codec: H.264 (libx264)
  - Profile: Constrained Baseline (Zero Latency)
  - GOP (Group of Pictures): 30 frames (Provides 1-second recovery time)
  - Payload Type: 96

🚀 lwIP Integration Guide

To receive these streams on an MCU using the lwIP Raw API, ensure your code handles the following:

- Broadcast Enable: Call ip_set_option(pcb, SOF_BROADCAST) on your UDP PCB to allow 192.168.0.255 packets.
- Port Separation:
  - Bind a PCB to Port 5004 for Video (H.264).
  - Bind a PCB to Port 5005 for Audio (ALAW).
- RTP Header: Strip the first 12 bytes of every incoming pbuf->payload to reach the raw media data.

📝 Usage Example

1. Run install_ffmpeg.bat to set up the environment.
2. Run broadcast_rtp_720p.bat to start the camera stream.
3. Open play_broadcast_rtp_720p.bat on another PC to verify the stream.
