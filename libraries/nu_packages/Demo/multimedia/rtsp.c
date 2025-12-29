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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "lwip/sockets.h"

#include "librtp.h"

#define RTSP_PORT_DEFAULT       554
#define RTSP_RX_TIMEOUT_MS      2000
#define RTSP_BUF_SIZE           2048 // Increased to handle larger SDPs
#define RTSP_CMD_SIZE           512
#define RTSP_URL_SIZE           256

/* Helper: Check if response code is 2xx */
static int rtsp_check_status(const char *resp)
{
    int code = 0;
    // Format usually: RTSP/1.0 200 OK
    if (sscanf(resp, "%*s %d", &code) != 1) return -1;
    if (code >= 200 && code < 300) return 0; // Success

    printf("RTSP: Error status code: %d\n", code);
    return -1;
}

/* Helper: Parse Session ID, handling optional ";timeout=xx" */
static void rtsp_parse_session_id(const char *resp, char *out)
{
    const char *p = strstr(resp, "Session:");
    if (!p) p = strstr(resp, "session:"); // Try lowercase just in case
    if (!p) return;

    p += strlen("Session:");
    while (*p == ' ') p++;

    int i = 0;
    // Copy until semicolon, newline, or carriage return
    while (*p && *p != ';' && *p != '\r' && *p != '\n' && i < 63)
    {
        out[i++] = *p++;
    }
    out[i] = '\0';
}

static int rtsp_send(int fd, const char *cmd)
{
    if (fd < 0) return -1;
    // printf(">> SEND:\n%s\n", cmd); // Debug print
    return lwip_write(fd, cmd, strlen(cmd));
}

static int rtsp_recv(int fd, char *buf, int size, int timeout_ms)
{
    fd_set rfds;
    struct timeval tv;

    if (fd < 0) return -1;

    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    memset(buf, 0, size);

    if (lwip_select(fd + 1, &rfds, NULL, NULL, &tv) > 0)
    {
        int ret = lwip_read(fd, buf, size - 1); // Leave room for null terminator
        if (ret > 0)
        {
            buf[ret] = 0;
            // printf("<< RECV:\n%s\n", buf); // Debug print
            return ret;
        }
    }
    return -1;
}

/* * SDP Parsing Logic
 * Handles:
 * 1. a=control:* (aggregate control)
 * 2. a=control:rtsp://... (absolute URL)
 * 3. a=control:trackID=1 (relative URL)
 */
void rtsp_parse_sdp_tracks(const char *sdp, const char *base_url,
                           char *track_url_video, char *track_url_audio)
{
    const char *line = sdp;
    int current_is_video = 0;
    int current_is_audio = 0;

    // By default, if parsing fails, audio/video urls might default to base_url
    // depending on implementation, but let's keep them empty to detect errors.

    while (line && *line)
    {
        const char *next = strstr(line, "\n");
        size_t len = next ? (size_t)(next - line) : strlen(line);

        // Basic line extraction
        char buf[256];
        if (len >= sizeof(buf)) len = sizeof(buf) - 1;
        memcpy(buf, line, len);
        buf[len] = 0;

        // Strip CR/LF
        char *cr = strchr(buf, '\r');
        if (cr) *cr = 0;

        // Detect Media Type
        if (strncmp(buf, "m=video", 7) == 0)
        {
            current_is_video = 1;
            current_is_audio = 0;
        }
        else if (strncmp(buf, "m=audio", 7) == 0)
        {
            current_is_video = 0;
            current_is_audio = 1;
        }

        // Detect Control URL
        if (strncmp(buf, "a=control:", 10) == 0)
        {
            const char *ctrl = buf + 10;
            char *dest = NULL;

            if (current_is_video) dest = track_url_video;
            else if (current_is_audio) dest = track_url_audio;

            if (dest)
            {
                // Case 1: Aggregate control ("*"), ignore or set to base if needed,
                // but usually we wait for specific track control.
                if (strcmp(ctrl, "*") == 0)
                {
                    // Some servers use * for the track if the base URL is the track URL.
                    // But usually we ignore this line in favor of specific tracks.
                }
                // Case 2: Absolute URL
                else if (strncmp(ctrl, "rtsp://", 7) == 0)
                {
                    strncpy(dest, ctrl, RTSP_URL_SIZE - 1);
                }
                // Case 3: Relative URL
                else
                {
                    int base_len = strlen(base_url);
                    int ctrl_len = strlen(ctrl);

                    if (base_len + ctrl_len + 2 < RTSP_URL_SIZE)
                    {
                        strcpy(dest, base_url);
                        // Ensure slash separator
                        if (base_url[base_len - 1] != '/')
                            strcat(dest, "/");
                        strcat(dest, ctrl);
                    }
                }
            }
        }

        line = next ? next + 1 : NULL;
    }
}

int rtsp_client_init(const char *ip, int base_rtp_port)
{
    int fd = -1;
    struct sockaddr_in addr = {0};
    char url[RTSP_URL_SIZE], cmd[RTSP_CMD_SIZE];
    char *buf = NULL; // Use heap for large Rx buffer
    char session_id[64] = {0};
    char track_url_video[RTSP_URL_SIZE] = {0};
    char track_url_audio[RTSP_URL_SIZE] = {0};
    int cseq = 1;
    int ret = -1;

    if (!ip || base_rtp_port <= 0)
    {
        printf("RTSP: invalid parameters\n");
        return -1;
    }

    // Allocate buffer on heap to save stack space
    buf = (char *)malloc(RTSP_BUF_SIZE);
    if (!buf) {
        printf("RTSP: malloc fail\n");
        return -1;
    }

    snprintf(url, sizeof(url), "rtsp://%s", ip);

    /* ---- Connect RTSP server ---- */
    fd = lwip_socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
    {
        printf("RTSP: socket create fail\n");
        goto fail;
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(RTSP_PORT_DEFAULT);
    addr.sin_addr.s_addr = inet_addr(ip);

    if (lwip_connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        printf("RTSP: connect fail\n");
        goto fail;
    }

    /* ---- OPTIONS ---- */
    snprintf(cmd, sizeof(cmd),
             "OPTIONS %s RTSP/1.0\r\n"
             "CSeq: %d\r\n\r\n",
             url, cseq++);
    if (rtsp_send(fd, cmd) < 0) goto fail;
    if (rtsp_recv(fd, buf, RTSP_BUF_SIZE, RTSP_RX_TIMEOUT_MS) <= 0) goto fail;
    if (rtsp_check_status(buf) != 0) goto fail;

    /* ---- DESCRIBE ---- */
    snprintf(cmd, sizeof(cmd),
             "DESCRIBE %s RTSP/1.0\r\n"
             "CSeq: %d\r\n"
             "Accept: application/sdp\r\n\r\n",
             url, cseq++);
    if (rtsp_send(fd, cmd) < 0) goto fail;
    if (rtsp_recv(fd, buf, RTSP_BUF_SIZE, RTSP_RX_TIMEOUT_MS) <= 0) goto fail;
    if (rtsp_check_status(buf) != 0) goto fail;

    /* ---- Parse SDP for track URLs ---- */
    rtsp_parse_sdp_tracks(buf, url, track_url_video, track_url_audio);

    if (strlen(track_url_video) == 0 && strlen(track_url_audio) == 0)
    {
        printf("RTSP: No supported tracks found in SDP\n");
        goto fail;
    }

    printf("Video track: %s\n", track_url_video);
    printf("Audio track: %s\n", track_url_audio);

    /* ---- SETUP Video ---- */
    if (track_url_video[0])
    {
        // Add Session header if we already have it (unlikely for first setup, but good practice)
        char session_header[128] = {0};
        if (session_id[0]) snprintf(session_header, sizeof(session_header), "Session: %s\r\n", session_id);

        if (!rtp_init(base_rtp_port, RTP_PT_H264))
        {
            printf("RTSP: RTP video init fail\n");
            goto fail;
        }

        snprintf(cmd, sizeof(cmd),
                 "SETUP %s RTSP/1.0\r\n"
                 "CSeq: %d\r\n"
                 "%s" // Optional Session Header
                 "Transport: RTP/AVP;unicast;client_port=%d-%d\r\n\r\n",
                 track_url_video, cseq++, session_header, base_rtp_port, base_rtp_port + 1);

        if (rtsp_send(fd, cmd) < 0) goto fail;
        if (rtsp_recv(fd, buf, RTSP_BUF_SIZE, RTSP_RX_TIMEOUT_MS) <= 0) goto fail;
        if (rtsp_check_status(buf) != 0) goto fail;

        // CRITICAL: Capture Session ID immediately after first successful SETUP
        if (session_id[0] == 0) rtsp_parse_session_id(buf, session_id);
    }

    /* ---- SETUP Audio ---- */
    if (track_url_audio[0])
    {
        // Must include Session ID now if video setup was successful
        char session_header[128] = {0};
        if (session_id[0]) snprintf(session_header, sizeof(session_header), "Session: %s\r\n", session_id);

        if (!rtp_init(base_rtp_port + 2, RTP_PT_PCMA))
        {
            printf("RTSP: RTP audio init fail\n");
            // Do not fail hard here, we can continue with just video?
            // For now, let's fail hard to be safe.
            goto fail;
        }

        snprintf(cmd, sizeof(cmd),
                 "SETUP %s RTSP/1.0\r\n"
                 "CSeq: %d\r\n"
                 "%s" // Session Header is MANDATORY if video setup succeeded
                 "Transport: RTP/AVP;unicast;client_port=%d-%d\r\n\r\n",
                 track_url_audio, cseq++, session_header, base_rtp_port + 2, base_rtp_port + 3);

        if (rtsp_send(fd, cmd) < 0) goto fail;
        if (rtsp_recv(fd, buf, RTSP_BUF_SIZE, RTSP_RX_TIMEOUT_MS) <= 0) goto fail;
        if (rtsp_check_status(buf) != 0) goto fail;

        if (session_id[0] == 0) rtsp_parse_session_id(buf, session_id);
    }

    if (session_id[0] == 0)
    {
        printf("RTSP: Failed to acquire Session ID\n");
        goto fail;
    }

    /* ---- PLAY ---- */
    snprintf(cmd, sizeof(cmd),
             "PLAY %s RTSP/1.0\r\n"
             "CSeq: %d\r\n"
             "Session: %s\r\n"
             "Range: npt=0.000-\r\n\r\n",
             url, cseq++, session_id);

    if (rtsp_send(fd, cmd) < 0) goto fail;
    if (rtsp_recv(fd, buf, RTSP_BUF_SIZE, RTSP_RX_TIMEOUT_MS) <= 0) goto fail;
    if (rtsp_check_status(buf) != 0) goto fail;

    printf("RTSP client started. Session: %s\n", session_id);

    free(buf);
    return fd;

fail:
    if (fd >= 0) lwip_close(fd);
    if (buf) free(buf);

    // Add logic here to de-init RTP if it was initialized above
    // rtp_deinit(base_rtp_port);
    // rtp_deinit(base_rtp_port + 2);

    return -1;
}
