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
#include "rtdevice.h"

#include "vc8000_lib.h"
#include "drv_common.h"
#include "drv_disp.h"

#define DBG_ENABLE
#define DBG_LEVEL DBG_LOG
#define DBG_SECTION_NAME  "vde"
#define DBG_COLOR
#include <rtdbg.h>

static int index = 0;
static uint32_t fbsize, fbaddr, fbpicese;
static rt_device_t psDevLcd = RT_NULL;
static rt_device_t s_psSndDev = RT_NULL;
static struct rt_device_graphic_info s_GfxInfo;
static int handle = -1;
#define DEF_LAYER_NAME    "lcd"
#define SOUND_DEVICE_NAME "sound0"

void h264_nalu_received(uint8_t *data, size_t len)
{
    /* Default example: write to stdout as annex-B (with start code) for testing */
    uint32_t remain = 0;

    if (handle < 0 || data == NULL || len == 0)
        return ;

    int ret = VC8000_Decode(handle, data, len, RT_NULL, &remain);
    if (ret != 0)
    {
        //rt_kprintf("[%s %d, %d] NALU size: %d, remain=%d\n", __func__, __LINE__, ret, len, remain);
    }
}

void audio_pcm16_play(int16_t *data, size_t len)
{
    if (s_psSndDev && data && len)
        rt_device_write(s_psSndDev, 0, (const void *)data, len);
}


static void pic_flush(VDE_FLUSH_CXT *psVDEFlushCtx)
{
    RT_ASSERT(psVDEFlushCtx != RT_NULL);

    if (psDevLcd && psVDEFlushCtx->u32FrameBufAddr && psVDEFlushCtx->u32FrameBufSize)
    {
        rt_device_control(psDevLcd, RTGRAPHIC_CTRL_PAN_DISPLAY, (void *)psVDEFlushCtx->u32FrameBufAddr);
    }
}


static rt_err_t sound_fini(void)
{
    if (s_psSndDev != RT_NULL)
    {
        rt_device_close(s_psSndDev);
        s_psSndDev = RT_NULL;
    }

    return RT_EOK;
}

static rt_err_t display_fini(void)
{
    if (handle >= 0)
    {
        VC8000_Close(handle);
        handle = -1;
    }

    if (psDevLcd != RT_NULL)
    {
        rt_device_close(psDevLcd);
        psDevLcd = RT_NULL;
    }

    return RT_EOK;
}

static rt_err_t display_init(void)
{
    rt_err_t ret;

    psDevLcd = rt_device_find(DEF_LAYER_NAME);
    if (psDevLcd == RT_NULL)
    {
        LOG_E("Can't find %s", DEF_LAYER_NAME);
        goto exit_vde_init;
    }

    /* Set pixel format to NV12 */
    int pixfmt = RTGRAPHIC_PIXEL_FORMAT_NV12;
    if (rt_device_control(psDevLcd, RTGRAPHIC_CTRL_SET_MODE, &pixfmt) != RT_EOK)
    {
        LOG_E("Can't get LCD info %s\n", DEF_LAYER_NAME);
        goto exit_vde_init;
    }

    /* Get LCD Info */
    ret = rt_device_control(psDevLcd, RTGRAPHIC_CTRL_GET_INFO, &s_GfxInfo);
    if (ret != RT_EOK)
    {
        LOG_E("Can't get LCD info %s", DEF_LAYER_NAME);
        goto exit_vde_init;
    }

    /* calculate frame buffer size */
    fbsize = (s_GfxInfo.width * s_GfxInfo.height * s_GfxInfo.bits_per_pixel / 8);
    fbpicese = s_GfxInfo.smem_len / fbsize;
    fbaddr = (uint32_t)s_GfxInfo.framebuffer;

    /* open lcd */
    ret = rt_device_open(psDevLcd, 0);
    if (ret != RT_EOK)
    {
        LOG_E("Can't open %s", DEF_LAYER_NAME);
        goto exit_vde_init;
    }

    /* open VC8000 decoder */
    handle = VC8000_Open(VC8000_CODEC_H264, pic_flush);
    if (handle < 0)
    {
        LOG_E("VC8000_Open failed! (%d)", handle);
        goto exit_vde_init;
    }

    /* set post-process parameters */
    {
        struct pp_params pp = {0};

        pp.frame_buf_w = s_GfxInfo.width;
        pp.frame_buf_h = s_GfxInfo.height;
        pp.img_out_w = s_GfxInfo.width;
        pp.img_out_h = s_GfxInfo.height;
        pp.img_out_fmt = VC8000_PP_F_NV12; // NV12 format
        pp.rotation = VC8000_PP_ROTATION_NONE; // no rotation
        pp.pp_out_dst = fbaddr; // point to frame buffer
        pp.pp_out_dst_bufsize = fbsize * fbpicese; // frame buffer size

        /* set post-process parameters */
        ret = VC8000_PostProcess(handle, &pp);
        if (ret < 0)
        {
            LOG_E("VC8000_PostProcess failed! (%d)\n", ret);
            goto exit_vde_init;
        }
    }

    LOG_I("LCD Width: %d",   s_GfxInfo.width);
    LOG_I("LCD Height: %d",  s_GfxInfo.height);
    LOG_I("LCD bpp:%d",   s_GfxInfo.bits_per_pixel);
    LOG_I("LCD pixel format:%d",   s_GfxInfo.pixel_format);
    LOG_I("LCD frame buffer@0x%08x",   s_GfxInfo.framebuffer);
    LOG_I("LCD frame buffer size:%d",   s_GfxInfo.smem_len);

    return RT_EOK;

exit_vde_init:

    display_fini();

    return -RT_ERROR;
}

static rt_err_t sound_init(void)
{
    struct rt_audio_caps caps;
    rt_err_t ret ;

    s_psSndDev = rt_device_find(SOUND_DEVICE_NAME);
    if (s_psSndDev == RT_NULL)
        goto exit_sound_init;

    // Just play sound, so set write only.
    ret = rt_device_open(s_psSndDev, RT_DEVICE_OFLAG_WRONLY);
    if (ret != RT_EOK)
    {
        goto exit_sound_init;
    }

    caps.main_type               = AUDIO_TYPE_OUTPUT;
    caps.sub_type                = AUDIO_DSP_PARAM;
    caps.udata.config.samplerate = 8000;
    caps.udata.config.channels   = 1;
    caps.udata.config.samplebits = 16;

    ret = rt_device_control(s_psSndDev, AUDIO_CTL_CONFIGURE, &caps);
    if (ret != RT_EOK)
    {
        goto exit_sound_init;
    }

exit_sound_init:

    sound_fini();

    return -RT_ERROR;
}

int render_init(void)
{
    sound_init();
    display_init();

    return 0;
}

int render_fini(void)
{
    sound_fini();
    display_fini();

    return 0;
}
