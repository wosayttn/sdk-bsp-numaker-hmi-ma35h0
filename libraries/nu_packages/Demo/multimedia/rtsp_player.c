/**************************************************************************//**
*
* @copyright (C) 2019 Nuvoton Technology Corp. All rights reserved.
*
* SPDX-License-Identifier: Apache-2.0
*
* Change Logs:
* Date            Author       Notes
* 2025-12-1       Wayne        First version
*
******************************************************************************/

#include "rtthread.h"

#define DBG_ENABLE
#define DBG_LEVEL DBG_LOG
#define DBG_SECTION_NAME  "rtp_mcast_player"
#define DBG_COLOR
#include <rtdbg.h>

#include "vc8000_lib.h"
#include "drv_common.h"
#include "drv_disp.h"
#include "librtp.h"
#include "render.h"

#define RTSP_SRV_IPADDR "192.168.0.149"
#define RTP_PORT 5004

#define RTSP_PORT_DEFAULT        554
#define RTSP_TCP_BUFFER_SIZE     2048
#define RTP_BUFFER_SIZE          2048

struct rtsp_client
{
    int rtsp_sock;
    int rtp_sock;

    char *rtsp_buf;
    char *rtp_buf;

    char session_id[64];
    int  client_rtp_port;

    rt_thread_t rtp_thread;
};

static int rtsp_tcp_fd = -1;

/* ---------------------------------------------------------------------------
 * Parse "Session: xxxxx"
 * ---------------------------------------------------------------------------*/
static void parse_session_id(const char *resp, char *out)
{
    const char *p = strstr(resp, "Session:");
    if (!p) return;

    p += strlen("Session:");
    while (*p == ' ') p++;

    sscanf(p, "%63s", out);
}

/* ---------------------------------------------------------------------------
 * RTSP Client Main
 * ---------------------------------------------------------------------------*/
/* Export thread to msh */
int rtsp_player_start(int argc, char **argv)
{
    if (render_init() == RT_EOK)
    {
        const char *unicast_addr = RTSP_SRV_IPADDR;
        uint16_t u16StartPort = RTP_PORT;

        if (argc > 1)
        {
            unicast_addr = (const char *)argv[1];
        }

        if (argc > 2)
        {
            u16StartPort = atoi(argv[2]);
        }

        rt_kprintf("%s: %s %d \n", __func__, unicast_addr, u16StartPort);

        if (rtsp_client_init(unicast_addr, u16StartPort) != 0)
        {
            rt_kprintf("rtsp_player_start failed\n");
            return -1;
        }
    }

    return 0;
}
MSH_CMD_EXPORT(rtsp_player_start, A RTSP client);
