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
/*
 * rtp_broadcast_player.c
 *
 * Complete LWIP-based RTP H.264 broadcast client example.
 * Targets: lwIP raw API, broadcast xxx.xxx.xxx.255/24:5004
 * Reassembles H.264 FU-A fragments into full NAL units.
 */

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

#define RTP_PORT 5004

static struct udp_pcb *broadcast_rtp_pcb_v = NULL;
static struct udp_pcb *broadcast_rtp_pcb_a = NULL;
static ip_addr_t broadcast_addr;

/* Stop and cleanup */
static void rtp_broadcast_fini(void)
{
    if (broadcast_rtp_pcb_v != NULL)
    {
        rtp_fini(broadcast_rtp_pcb_v);
        broadcast_rtp_pcb_v = NULL;
    }

    if (broadcast_rtp_pcb_a != NULL)
    {
        rtp_fini(broadcast_rtp_pcb_a);
        broadcast_rtp_pcb_a = NULL;
    }
}

/* Public initialization function: create PCB, bind, join broadcast group */
static int rtp_broadcast_init(uint16_t u16Port)
{
    broadcast_rtp_pcb_v = rtp_init(u16Port, RTP_PT_H264);
    if (broadcast_rtp_pcb_v == NULL)
    {
        rt_kprintf("Failed to claim UDP port-%d\n", u16Port);
        goto exit_rtp_broadcast_init;
    }

    broadcast_rtp_pcb_a = rtp_init(u16Port + 1, RTP_PT_PCMA);
    if (broadcast_rtp_pcb_a == NULL)
    {
        rt_kprintf("Failed to claim UDP port-%d\n", u16Port + 1);
        goto exit_rtp_broadcast_init;
    }

    rt_kprintf("RTP broadcast initialized on port %d for H.264 and port %d for G711.\n", u16Port, u16Port + 1);

    return 0;

exit_rtp_broadcast_init:

    rtp_broadcast_fini();

    return -1;
}

static int rtp_broadcast_stop(void)
{
    rtp_broadcast_fini();

    render_fini();

    return 0;
}

static int rtp_broadcast_start(int argc, char **argv)
{
    if (render_init() == RT_EOK)
    {
        uint16_t u16StartPort = RTP_PORT;

        if (argc > 1)
        {
            u16StartPort = atoi(argv[1]);
        }

        if (rtp_broadcast_init(u16StartPort) != 0)
        {
            rt_kprintf("rtp_broadcast_start failed\n");
            return -1;
        }

    }

    return 0;
}

static int rtp_broadcast_autostart(void)
{
    return rtp_broadcast_start(0, NULL);
}

//INIT_APP_EXPORT(rtp_broadcast_autostart);
MSH_CMD_EXPORT(rtp_broadcast_start, start RTP broadcast player);
