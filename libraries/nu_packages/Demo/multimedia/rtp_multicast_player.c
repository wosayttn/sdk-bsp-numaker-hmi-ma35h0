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
 * rtp_multicast_h264_player.c
 *
 * Complete LWIP-based RTP H.264 multicast client example.
 * Targets: lwIP raw API, IGMP join, multicast 224.1.1.1:5004
 * Reassembles H.264 FU-A fragments into full NAL units.
 *
 * Usage:
 *  - Enable LWIP_IGMP in lwipopts.h
 *  - Provide a netif_default (or change to specific netif)
 *  - Implement `h264_nalu_received(uint8_t *data, size_t len)` to consume NALUs
 *
 * lwipopts.h reminder (must enable IGMP):
 * #define LWIP_IGMP 1
 * #define IP_MULTICAST_TTL 1
 * #define IP_MULTICAST_LOOP 1
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
#define MCAST_ADDR "224.1.1.1"

static struct udp_pcb *multicast_rtp_pcb_v = NULL;
static struct udp_pcb *multicast_rtp_pcb_a = NULL;
static ip_addr_t multicast_addr;

/* Stop and cleanup */
static void rtp_multicast_fini(void)
{
    if (multicast_rtp_pcb_v != NULL)
    {
        rtp_fini(multicast_rtp_pcb_v);
        multicast_rtp_pcb_v = NULL;
    }

    if (multicast_rtp_pcb_a != NULL)
    {
        rtp_fini(multicast_rtp_pcb_a);
        multicast_rtp_pcb_a = NULL;
    }

    if (netif_default != NULL)
    {
        struct netif *netif = netif_default;
        err_t err = igmp_leavegroup(&netif->ip_addr, &multicast_addr);
        if (err != ERR_OK)
        {
            rt_kprintf("igmp_leavegroup failed: %d\n", err);
        }
    }
}

/* Public initialization function: create PCB, bind, join multicast group */
static int rtp_multicast_init(const char *mcast_addr, uint16_t u16Port)
{
    err_t err;

    if (ipaddr_aton(mcast_addr, &multicast_addr) == 0)
    {
        rt_kprintf("ipaddr_aton\n");

        goto exit_rtp_multicast_init;
    }
    else
    {
        if (!netif_default)
        {
            rt_kprintf("No default netif to join multicast\n");

            /* Keep running without IGMP join - maybe raw multicast still arrives */
            goto exit_rtp_multicast_init;
        }
        struct netif *netif = netif_default;

        err = igmp_joingroup(&netif->ip_addr, &multicast_addr);
        if (err != ERR_OK)
        {
            rt_kprintf("igmp_joingroup failed: %d\n", err);

            /* Not fatal for some environments */
            goto exit_rtp_multicast_init;
        }
        else
        {
            rt_kprintf("Joined multicast %s:%d\n", MCAST_ADDR, u16Port);

            multicast_rtp_pcb_v = rtp_init(u16Port, RTP_PT_H264);
            if (multicast_rtp_pcb_v == NULL)
            {
                rt_kprintf("Failed to claim UDP port-%d\n", u16Port);

                goto exit_rtp_multicast_init;
            }

            multicast_rtp_pcb_a = rtp_init(u16Port + 1, RTP_PT_PCMA);
            if (multicast_rtp_pcb_a == NULL)
            {
                rt_kprintf("Failed to claim UDP port-%d\n", u16Port + 1);
                goto exit_rtp_multicast_init;
            }

            rt_kprintf("RTP multicast initialized on port %d for H.264 and port %d for G711.\n", u16Port, u16Port + 1);
        }

    } //else

    return 0;

exit_rtp_multicast_init:

    rtp_multicast_fini();

    return -1;
}

static int rtp_multicast_stop(void)
{
    rtp_multicast_fini();

    render_fini();

    return 0;
}

static int rtp_multicast_start(int argc, char **argv)
{
    if (render_init() == RT_EOK)
    {
        const char *mcast_addr = MCAST_ADDR;
        uint16_t u16StartPort = RTP_PORT;

        if (argc > 1)
        {
            mcast_addr = (const char *)argv[1];
        }

        if (argc > 2)
        {
            u16StartPort = atoi(argv[2]);
        }

        rt_kprintf("%s: %s %d \n", __func__, mcast_addr, u16StartPort);

        if (rtp_multicast_init(mcast_addr, u16StartPort) != 0)
        {
            rt_kprintf("rtp_multicast_start failed\n");
            return -1;
        }

    }

    return 0;
}

//INIT_APP_EXPORT(rtp_multicast_start);
MSH_CMD_EXPORT(rtp_multicast_start, start RTP multicast player);
