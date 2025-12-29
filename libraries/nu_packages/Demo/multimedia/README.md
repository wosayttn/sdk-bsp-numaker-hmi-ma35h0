# RT-Thread MSH RTP/RTSP Player Commands

This document describes the FinSH/MSH commands available for controlling the RTP and RTSP player on RT-Thread. These commands allow the device to act as a receiver for network audio and video streams.

📋 Command Overview

| Command | Description | Network Mode |
| -- | -- | -- |
| rtp_multicast_start | Starts an RTP player using Multicast. | Multicast (Group) |
| rtp_broadcast_start | Starts an RTP player using Broadcast. | Broadcast (Subnet) |
| rtsp_player_start | Starts an RTSP client to pull a stream. | Unicast (Point-to-Point) |

🛠️ Usage Details

1. ``rtp_multicast_start``

    Starts the receiver and joins a specific multicast group to receive RTP packets.
    - Syntax: rtp_multicast_start [mcast_addr] [port]
    - Arguments:
    - mcast_addr: (Optional) The multicast IP address (e.g., 224.1.1.1). Defaults to MCAST_ADDR.
    - port: (Optional) The starting UDP port. Defaults to RTP_PORT.
    - Example:

      ```bash
      msh /> rtp_multicast_start 224.1.1.1 5004
      [7581] I/i2s tshell: Stop replay.
      [7583] I/i2s tshell: Close I2S.
      [7617] I/acodec.nau8822 tshell: Software Reset.

      [7657] I/acodec.nau8822 tshell: Initialized done.

      [7663] I/i2s tshell: Open I2S.
      [7665] W/i2s tshell: Real sample rate: 8007 Hz != preferred sample rate: 8000 Hz

      [7674] I/i2s tshell: Start replay.
      [7677] I/vde tshell: LCD Width: 1024
      [7681] I/vde tshell: LCD Height: 600
      [7685] I/vde tshell: LCD bpp:16
      [7689] I/vde tshell: LCD pixel format:13
      [7694] I/vde tshell: LCD frame buffer@0xc1244780
      [7699] I/vde tshell: LCD frame buffer size:7372800
      rtp_multicast_start: 224.1.1.1 5004
      Joined multicast 224.1.1.1:5004
      RTP: Initialized on port 5004 for payload type 96
      RTP: Initialized on port 5005 for payload type 8
      RTP multicast initialized on port 5004 for H.264 and port 5005 for G711.
      ```

2. ``rtp_broadcast_start``

    Starts the receiver to listen for UDP broadcast packets on the local subnet.
    - Syntax: rtp_broadcast_start [port]
    - Arguments:
    - port: (Optional) The UDP port to bind to. Defaults to RTP_PORT.
    - Example:

      ```bash
      msh /> rtp_broadcast_start 5004
      [3415] I/i2s tshell: Stop replay.
      [3417] I/i2s tshell: Close I2S.
      [3451] I/acodec.nau8822 tshell: Software Reset.

      [3491] I/acodec.nau8822 tshell: Initialized done.

      [3497] I/i2s tshell: Open I2S.
      [3499] W/i2s tshell: Real sample rate: 8007 Hz != preferred sample rate: 8000 Hz

      [3508] I/i2s tshell: Start replay.
      [3511] I/vde tshell: LCD Width: 1024
      [3515] I/vde tshell: LCD Height: 600
      [3519] I/vde tshell: LCD bpp:16
      [3523] I/vde tshell: LCD pixel format:13
      [3528] I/vde tshell: LCD frame buffer@0xc1244780
      [3533] I/vde tshell: LCD frame buffer size:7372800
      RTP: Initialized on port 5004 for payload type 96
      RTP: Initialized on port 5005 for payload type 8
      RTP broadcast initialized on port 5004 for H.264 and port 5005 for G711.
      ```

3. ``rtsp_player_start``

    Initiates an RTSP client session to connect to a remote RTSP server and stream media.
    - Syntax: rtsp_player_start [server_ip] [port]
    - Arguments:
    - server_ip: (Optional) The IP address of the RTSP server. Defaults to RTSP_SRV_IPADDR.
    - port: (Optional) The server port. Defaults to RTP_PORT(UDP).
    - Example:

      ```Bash
      msh /> rtsp_player_start 192.168.0.149 5004
      [3984] I/i2s tshell: Stop replay.
      [3987] I/i2s tshell: Close I2S.
      [4021] I/acodec.nau8822 tshell: Software Reset.

      [4061] I/acodec.nau8822 tshell: Initialized done.

      [4067] I/i2s tshell: Open I2S.
      [4069] W/i2s tshell: Real sample rate: 8007 Hz != preferred sample rate: 8000 Hz

      [4078] I/i2s tshell: Start replay.
      [4081] I/vde tshell: LCD Width: 1024
      [4085] I/vde tshell: LCD Height: 600
      [4089] I/vde tshell: LCD bpp:16
      [4093] I/vde tshell: LCD pixel format:13
      [4098] I/vde tshell: LCD frame buffer@0xc1244780
      [4103] I/vde tshell: LCD frame buffer size:7372800
      rtsp_player_start: 192.168.0.149 5004
      Video track: rtsp://192.168.0.149/trackID=0
      Audio track: rtsp://192.168.0.149/trackID=1
      RTP: Initialized on port 5004 for payload type 96
      RTP: Initialized on port 5006 for payload type 8
      RTSP client started. Session: 276265206036876
      ```
