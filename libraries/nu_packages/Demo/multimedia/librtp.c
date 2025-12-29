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

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "lwip/udp.h"
#include "lwip/igmp.h"
#include "lwip/ip_addr.h"
#include "lwip/pbuf.h"
#include "lwip/netif.h"
#include "lwip/err.h"
#include "lwip/sockets.h"

#include "librtp.h"

/* Helper: copy pbuf chain payload into linear buffer starting at offset
 * returns number of bytes copied
 */
size_t rtp_payload_copy(const struct pbuf *p, size_t offset, uint8_t *dst, size_t dst_max)
{
    size_t copied = 0;
    size_t remaining = dst_max;
    const struct pbuf *q = p;
    size_t pos = 0;

    while (q && remaining > 0)
    {
        /* each pbuf holds q->len bytes in q->payload */
        if (offset < pos + q->len)
        {
            /* start copying from inside this q */
            size_t start_in_q = (offset > pos) ? (offset - pos) : 0;
            size_t can_copy = q->len - start_in_q;
            if (can_copy > remaining)
            {
                can_copy = remaining;
            }

            memcpy(dst + copied, (uint8_t *)q->payload + start_in_q, can_copy);

            copied += can_copy;
            remaining -= can_copy;
            offset += can_copy;
        }

        pos += q->len;
        q = q->next;
    }

    return copied;
}

/* Handler called by udp_recv when a pbuf arrives */
static void rtp_udp_recv(void *arg, struct udp_pcb *pcb, struct pbuf *p,
                         const ip_addr_t *addr, u16_t port)
{
    rtp_header_t rtp_header;

    /* total packet length is p->tot_len, payload may be in chained pbufs */
    if ((p == NULL) || (p->tot_len < sizeof(rtp_header_t)))
    {
        goto exit_rtp_udp_recv;
    }

    size_t got = rtp_payload_copy(p, 0, (uint8_t *)&rtp_header, sizeof(rtp_header_t));
    if ((got < sizeof(rtp_header_t)) ||
            (rtp_header.v != 2))
    {
        /* not RTPv2 */
        goto exit_rtp_udp_recv;
    }

    /* compute CSRC count and extension if present (basic handling) */
    size_t rtp_header_len = sizeof(rtp_header_t) + rtp_header.cc * 4;

    /* If extension bit set, parse extension length (16-bit words) */
    if ((rtp_header.x) != 0)
    {
        size_t header_ext_len = 0;

        /* extension present - need to read extension header's length field */
        /* extension header starts after basic header and CSRC list */
        uint8_t ext_header[4];

        if (rtp_payload_copy(p, rtp_header_len, ext_header, 4) < 4)
        {
            goto exit_rtp_udp_recv;
        }

        uint16_t ext_len_words = (ext_header[2] << 8) | ext_header[3];
        header_ext_len = 4 + ext_len_words * 4;
        rtp_header_len += header_ext_len;
    }

    /* Ensure we have the full computed rtp_header_len */
    if (p->tot_len <= rtp_header_len)
    {
        goto exit_rtp_udp_recv;
    }

    /* Allocate a linear buffer for payload (small optimization: process directly from pbuf could be done)
     * but here we copy to ensure contiguous payload for parsing.
     */
    size_t payload_len = p->tot_len - rtp_header_len;
    if (payload_len <= 0)
        goto exit_rtp_udp_recv;

    switch (rtp_header.pt)
    {
    case RTP_PT_H264:
        /* Dynamic payload type for H.264 */
        rtp_h264_process((rtp_ctx_t *)arg, p, &rtp_header, rtp_header_len, payload_len);
        break;

    case RTP_PT_PCMU:
    case RTP_PT_PCMA:
        /* Dynamic payload type for G.711 */
        rtp_g711_process((rtp_ctx_t *)arg, p, &rtp_header, rtp_header_len, payload_len);
        break;

    default:
        /* Unsupported payload type */
        goto exit_rtp_udp_recv;
    }

exit_rtp_udp_recv:

    if (p != NULL)
        pbuf_free(p);
}

struct udp_pcb *rtp_init(uint16_t port, rtp_payload_type_t pt)
{
    struct udp_pcb *rtp_pcb;

    if ((rtp_pcb = udp_new()) == NULL)
    {
        printf("RTP: udp_new() failed\n");
        goto exit_rtp_udp_init;
    }

    // Explicitly tell this PCB it is allowed to receive broadcast packets
    ip_set_option(rtp_pcb, SOF_BROADCAST);

    if (udp_bind(rtp_pcb, IP_ADDR_ANY, port) != ERR_OK)
    {
        printf("RTP: udp_bind() failed\n");
        udp_remove(rtp_pcb);
        goto exit_rtp_udp_init;
    }

    rtp_ctx_t *pRTPCtx = (rtp_ctx_t *)malloc(sizeof(rtp_ctx_t));
    if (pRTPCtx == NULL)
    {
        printf("RTP: malloc() failed\n");
        goto exit_rtp_udp_init;
    }

    memset(pRTPCtx, 0, sizeof(rtp_ctx_t));
    pRTPCtx->payload_type = pt;
    pRTPCtx->pcb = rtp_pcb;

    /* Set the recv callback function. */
    udp_recv(rtp_pcb, rtp_udp_recv, (void *)pRTPCtx);

    printf("RTP: Initialized on port %d for payload type %d\n", port, pt);

    return rtp_pcb;

exit_rtp_udp_init:

    if (rtp_pcb != NULL)
    {
        udp_remove(rtp_pcb);
    }

    return NULL;
}

/* Stop and cleanup */
void rtp_fini(struct udp_pcb *rtp_pcb)
{
    if (rtp_pcb)
    {
        rtp_ctx_t *pRTPCtx = (rtp_ctx_t *)rtp_pcb->recv_arg;
        if (pRTPCtx == NULL)
        {
            free(pRTPCtx);
        }

        udp_remove(rtp_pcb);
    }
}
