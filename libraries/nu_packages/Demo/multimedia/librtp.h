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

#ifndef _LIBRTP_H
#define _LIBRTP_H

#include "lwip/udp.h"
#include "lwip/igmp.h"
#include "lwip/ip_addr.h"
#include "lwip/pbuf.h"
#include "lwip/netif.h"
#include "lwip/err.h"
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

typedef enum
{
    /* --------------------------------------------------------------------
     * Static RTP Payload Types (0–95)
     * Defined by RFC 3551
     * ------------------------------------------------------------------*/

    /* Audio */
    RTP_PT_PCMU          = 0,    /* G.711 µ-law */
    RTP_PT_GSM           = 3,
    RTP_PT_G723          = 4,
    RTP_PT_DVI4_8000     = 5,
    RTP_PT_DVI4_16000    = 6,
    RTP_PT_LPC           = 7,
    RTP_PT_PCMA          = 8,    /* G.711 A-law */
    RTP_PT_G722          = 9,
    RTP_PT_L16_STEREO    = 10,
    RTP_PT_L16_MONO      = 11,
    RTP_PT_QCELP         = 12,
    RTP_PT_CN            = 13,   /* Comfort Noise */
    RTP_PT_MPA           = 14,   /* MPEG Audio */
    RTP_PT_G728          = 15,
    RTP_PT_DVI4_11025    = 16,
    RTP_PT_DVI4_22050    = 17,
    RTP_PT_G729          = 18,

    /* Video */
    RTP_PT_CELB          = 25,
    RTP_PT_JPEG          = 26,   /* MJPEG / JPEG */
    RTP_PT_NV            = 28,
    RTP_PT_H261          = 31,
    RTP_PT_MPV           = 32,   /* MPEG-1/2 Video */
    RTP_PT_MP2T          = 33,   /* MPEG-TS */
    RTP_PT_H263          = 34,

    /* Reserved static range ends at 95 */
    /* --------------------------------------------------------------------
     * Dynamic RTP Payload Types (96–127)
     * Usually defined via SDP (RTSP)
     * ------------------------------------------------------------------*/

    /* Common assignments (not guaranteed!) */
    RTP_PT_H264          = 96,   /* Most common for H.264 */
    RTP_PT_H265          = 97,   /* Many IP cameras use 97 for HEVC */
    RTP_PT_AAC           = 98,   /* Common for AAC audio */
    RTP_PT_OPUS          = 99,
    RTP_PT_VP8           = 100,
    RTP_PT_VP9           = 101,

    /* User can map remaining 102–127 as needed */

} rtp_payload_type_t;

#pragma pack(push, 1)
typedef struct
{
    /* Byte 0 */
    uint8_t cc: 4;      /* CSRC count            */
    uint8_t x: 1;       /* Header extension flag */
    uint8_t p: 1;       /* Padding flag          */
    uint8_t v: 2;       /* RTP version (always 2)*/

    /* Byte 1 */
    uint8_t pt: 7;      /* Payload Type          */
    uint8_t m: 1;       /* Marker bit            */

    /* Bytes 2–3 */
    uint8_t seq[2];       /* Sequence number       */

    /* Bytes 4–7 */
    uint8_t timestamp[4]; /* Timestamp             */

    /* Bytes 8–11 */
    uint8_t ssrc[4];      /* SSRC identifier       */

    /* Following fields are variable:
       - uint32_t csrc_list[cc];     (if CC > 0)
       - RTP header extension block  (if X = 1)
    */
} rtp_header_t;
#pragma pack(pop)

typedef struct
{
    rtp_payload_type_t payload_type;
    struct udp_pcb *pcb;
    void *private_data;
} rtp_ctx_t;

/* RTP common functions */
struct udp_pcb *rtp_init(uint16_t port, rtp_payload_type_t pt);
void rtp_fini(struct udp_pcb *rtp_pcb);
size_t rtp_payload_copy(const struct pbuf *p, size_t offset, uint8_t *dst, size_t dst_max);

/* RTP PT process entry */
void rtp_h264_process(rtp_ctx_t *ctx, struct pbuf *p, rtp_header_t *rtp_header, size_t rtp_header_len, size_t payload_len);
void rtp_g711_process(rtp_ctx_t *ctx, struct pbuf *p, rtp_header_t *rtp_header, size_t rtp_header_len, size_t payload_len);

/* RTSP */
int rtsp_client_init(const char *ip, int rtp_port);


#endif /* _LIBRTP_H */
