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

#include "librtp.h"

__attribute__((weak)) void audio_pcm16_play(int16_t *data, size_t len)
{
    printf("[%s %d] PCM16 size: %d\n", __func__, __LINE__, len);
}

/* -------------------------------
 *  A-LAW → PCM16
 * ------------------------------- */
static int16_t alaw_to_pcm16(uint8_t a_val)
{
    a_val ^= 0x55;

    int t = (a_val & 0x0F) << 4;
    int seg = ((unsigned)a_val & 0x70) >> 4;

    switch (seg)
    {
    case 0:
        t += 8;
        break;
    case 1:
        t += 0x108;
        break;
    default:
        t += 0x108;
        t <<= (seg - 1);
        break;
    }

    return (a_val & 0x80) ? t : -t;
}

/* -------------------------------
 *  μ-LAW → PCM16
 * ------------------------------- */
static int16_t ulaw_to_pcm16(uint8_t u_val)
{
    u_val = ~u_val;

    int sign = (u_val & 0x80);
    int exponent = (u_val >> 4) & 0x07;
    int mantissa = u_val & 0x0F;

    int t = ((mantissa << 1) + 33) << exponent;
    return sign ? -(t - 33) : (t - 33);
}

/* -------------------------------
 *  Decode RTP G.711 → PCM16
 * ------------------------------- */
static size_t g711_to_pcm16(const uint8_t *payload,
                            size_t payload_len,
                            uint8_t pt,
                            int16_t *pcm_out)
{
    size_t samples = payload_len;
    size_t i;
    if (pt == RTP_PT_PCMU)              // PCMU (µ-law)
    {
        for (i = 0; i < samples; i++)
        {
            uint8_t v = payload[i];
            pcm_out[i] = ulaw_to_pcm16(v);
        }
    }
    else if (pt == RTP_PT_PCMA)         // PCMA (A-law)
    {
        for (i = 0; i < samples; i++)
        {
            uint8_t v = payload[i];
            pcm_out[i] = alaw_to_pcm16(v);
        }
    }
    else
    {
        // Unsupported payload type
        memset(pcm_out, 0, samples * sizeof(int16_t));
    }

    return samples * 2; /* bytes */
}

void rtp_g711_process(rtp_ctx_t *ctx,
                      struct pbuf *p,
                      rtp_header_t *rtp_header,
                      size_t rtp_header_len,
                      size_t payload_len)
{
    size_t offset = rtp_header_len;     /* byte offset to RTP payload */
    size_t remaining = payload_len;
    const struct pbuf *q = p;

    /* Enough for 20ms G.711 (160 samples), times 2 for 16-bit PCM */
    int16_t pcm_buf[160];

    size_t pos = 0;

    while (q && remaining > 0)
    {
        /* q->len bytes exist at q->payload */
        if (offset < pos + q->len)
        {
            /* determine where inside this pbuf to start */
            size_t start_in_q = (offset > pos) ? (offset - pos) : 0;

            /* max bytes available inside this pbuf */
            size_t can_copy = q->len - start_in_q;

            /* clip to remaining payload */
            if (can_copy > remaining)
                can_copy = remaining;

            /* clip to pcm_buf capacity (160 samples) */
            if (can_copy > sizeof(pcm_buf)/2)
                can_copy = sizeof(pcm_buf)/2;

            /* Convert G.711 A-law / μ-law to PCM16 */
            g711_to_pcm16(
                (uint8_t *)q->payload + start_in_q,
                can_copy,
                rtp_header->pt,
                pcm_buf
            );

            /* Output PCM16 (each sample = 2 bytes) */
            audio_pcm16_play(pcm_buf, can_copy * sizeof(int16_t));

            remaining -= can_copy;
            offset += can_copy;
        }

        pos += q->len;
        q = q->next;
    }

}
