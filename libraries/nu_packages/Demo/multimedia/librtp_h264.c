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

#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "librtp.h"

/* Configurable limits */
#define MAX_NALU_SIZE    (1024 * 1024)           /* 1MB max NALU */

/* H.264 reassembly state (simple single-SSRC) */
typedef struct
{
    uint32_t ssrc;
    uint32_t last_seq;
    uint32_t assembling;     /* 0 = idle, 1 = assembling FU-A */
    uint32_t buf_len;        /* length stored in buf */
    uint8_t *buf;           /* allocated buffer for NALU */
} h264_reassembly_t;

typedef struct
{
    uint8_t type : 5;     // bits 0-4
    uint8_t nri  : 2;     // bits 5-6
    uint8_t f    : 1;     // bit  7
} h264_nal_header_t;

__attribute__((weak)) void h264_nalu_received(uint8_t *data, size_t len)
{
    printf("[%s %d] NALU size: %d\n", __func__, __LINE__, len);
}

/* Ensure reassembly buffer exists */
static int h264_state_create_reassembly_buf(h264_reassembly_t *state)
{
    if (state && (state->buf == NULL))
    {
        state->buf = (uint8_t *)rt_malloc_align(RT_ALIGN(MAX_NALU_SIZE, 64), 64);
        if (state->buf == NULL)
        {
            return -1;
        }

        state->buf_len = 0;
    }

    return 0;
}

/* Ensure reassembly buffer exists */
static int h264_state_destroy_reassembly_buf(h264_reassembly_t *state)
{
    if (state && (state->buf != NULL))
    {
        rt_free_align(state->buf);
        state->buf = NULL;
        state->buf_len = 0;
    }

    return 0;
}

/* Reset FU-A assembly state */
static void h264_state_reset_reassembly(h264_reassembly_t *state)
{
    state->assembling = 0;
    state->buf_len = 0;
    state->last_seq = 0;
}

/* Process a single RTP packet payload containing H.264 data (after RTP header)
 * - payload points to the first byte after RTP header (may be inside pbuf chain)
 * - payload_len is the size of the payload (total bytes after RTP header)
 * - marker, seq, ssrc used for stream control
 */
void rtp_h264_process(rtp_ctx_t *ctx, struct pbuf *p, rtp_header_t *rtp_header, size_t rtp_header_len, size_t payload_len)
{
    h264_reassembly_t *state;

    if (ctx->private_data == NULL)
    {
        state = (void *)malloc(sizeof(h264_reassembly_t));
        if (state == NULL)
        {
            return;
        }
        memset(state, 0, sizeof(h264_reassembly_t));
        /* If we were assembling, discard previous */
        if (h264_state_create_reassembly_buf(state) < 0)
        {
            goto exit_process_rtp_h264;
        }
        h264_state_reset_reassembly(state);
        ctx->private_data = state;
    }
    state = (h264_reassembly_t *)ctx->private_data;

    uint8_t fu[2];
    if (rtp_payload_copy(p, rtp_header_len, &fu[0], sizeof(fu)) < sizeof(fu))
    {
        goto exit_process_rtp_h264;
    }

    h264_nal_header_t *h264_nal_hdr = (h264_nal_header_t *)&fu[0];
    uint8_t nal_type = h264_nal_hdr->type;

    //rt_kprintf("RTP H264 payload: nal_type=%d, payload_len=%d\n", nal_type, payload_len);

    switch (nal_type)
    {
    case 28:
    {
        /* Fragmentation unit (FU-A) type 28 */
        if (payload_len < 2)
        {
            goto exit_process_rtp_h264;
        }

        uint16_t seq = (rtp_header->seq[0] << 8) | rtp_header->seq[1];

        uint32_t timestamp = (rtp_header->timestamp[0] << 24) |
                             (rtp_header->timestamp[1] << 16) |
                             (rtp_header->timestamp[2] << 8)  |
                             (rtp_header->timestamp[3] << 0);

        uint32_t ssrc = (rtp_header->ssrc[0] << 24) |
                        (rtp_header->ssrc[1] << 16) |
                        (rtp_header->ssrc[2] << 8)  |
                        (rtp_header->ssrc[3] << 0);


        uint8_t fu_indicator = fu[0];
        uint8_t fu_header    = fu[1];

        uint8_t F   = (fu_indicator >> 7) & 0x01;
        uint8_t NRI = (fu_indicator >> 5) & 0x03;
        uint8_t fu_type = fu_indicator & 0x1F;   // Should be 28 or 29

        uint8_t S   = (fu_header >> 7) & 0x01;
        uint8_t E   = (fu_header >> 6) & 0x01;
        uint8_t R   = (fu_header >> 5) & 0x01;
        uint8_t orig_nal_type = fu_header & 0x1F;

        if (state->assembling &&
                state->ssrc != 0 &&
                state->ssrc != ssrc)
        {
            h264_state_reset_reassembly(state);
        }

        /* Start of fragmented NALU */
        if (S)
        {

            /* Recreate NAL header: F | NRI from FU indicator, nal type from fu_header */
            uint8_t nal_hdr = (fu_indicator & 0xE0) | (orig_nal_type & 0x1F);
            state->buf[state->buf_len++] = nal_hdr;

            size_t to_copy = payload_len - sizeof(fu);
            if (state->buf_len + to_copy > MAX_NALU_SIZE)
            {
                goto exit_process_rtp_h264;
            }

            if (rtp_payload_copy(p, rtp_header_len + sizeof(fu), state->buf + state->buf_len, to_copy) < to_copy)
            {
                goto exit_process_rtp_h264;
            }

            state->buf_len += to_copy;

            state->assembling = 1;
            state->last_seq = seq;
            state->ssrc = ssrc;

            /* If a single FU-A packet had both S and E set (rare), finish now */
            if (E)
            {
                /* Complete NALU ready */
                h264_nalu_received(state->buf, state->buf_len);
                goto exit_process_rtp_h264;
            }
        }
        else
        {
            /* Middle or End fragments */
            if (state->assembling)
            {
                /* Check sequence continuity (basic) */
                uint16_t expected_seq = state->last_seq + 1;
                if (seq != expected_seq)
                {
                    /* Packet loss detected - drop current assembly */
                    goto exit_process_rtp_h264;
                }

                /* Append payload (skip FU indicator + FU header) */
                size_t to_copy = payload_len - sizeof(fu);
                if ((state->buf_len + to_copy > MAX_NALU_SIZE) ||
                        (rtp_payload_copy(p, rtp_header_len + sizeof(fu), state->buf + state->buf_len, to_copy) < to_copy))
                {
                    goto exit_process_rtp_h264;
                }

                state->buf_len += to_copy;
                state->last_seq = seq;

                if (E)
                {
                    /* End of NALU -> pass to callback */
                    h264_nalu_received(state->buf, state->buf_len);
                    goto exit_process_rtp_h264;
                }
            }

        } // if(S)-else
    }
    break;

    case 24:
    {
        /* STAP-A (Single-time aggregation packet) - type 24 */
        /* STAP-A contains multiple NALUs with 16-bit length prefixes */

        /* Single NAL unit packet (1-23) */
        if (rtp_payload_copy(p, rtp_header_len, state->buf, payload_len) < payload_len)
        {
            goto exit_process_rtp_h264;
        }

        size_t pos = 1; /* skip STAP-A header */
        while (pos + 2 <= payload_len)
        {
            uint16_t nalu_size = (state->buf[pos] << 8) | state->buf[pos + 1];
            pos += 2;

            if (nalu_size == 0 || pos + nalu_size > payload_len)
                break;

            /* callback with pointer to payload[pos] length nalu_size */
            h264_nalu_received(state->buf + pos, nalu_size);
            pos += nalu_size;
        }
    }
    break;

    case 1 ... 23:
    default:
    {
        /* Single NAL unit packet (1-23) */
        if (rtp_payload_copy(p, rtp_header_len, state->buf, payload_len) < payload_len)
        {
            goto exit_process_rtp_h264;
        }
        h264_nalu_received(state->buf, payload_len);
    }
    break;

    }

    return;

exit_process_rtp_h264:

    h264_state_reset_reassembly(state);

    return;
}
