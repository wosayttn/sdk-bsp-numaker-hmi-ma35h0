/**************************************************************************//**
*
* @copyright (C) 2019 Nuvoton Technology Corp. All rights reserved.
*
* SPDX-License-Identifier: Apache-2.0
*
* Change Logs:
* Date            Author       Notes
* 2023-4-26       Wayne        First version
*
******************************************************************************/

#if 0
//Disable the program due to RAM usage.
#include <rtthread.h>

#if defined(BSP_USING_VDE)

#include "vc8000_lib.h"
#include "drv_common.h"
#include "drv_disp.h"

#include <dfs_posix.h>

#define DBG_ENABLE
#define DBG_LEVEL DBG_LOG
#define DBG_SECTION_NAME  "vde"
#define DBG_COLOR
#include <rtdbg.h>

#define PATH_H264_INCBIN          ".//default.264"
#define PATH_JPEG_INCBIN          ".//default.jpg"

INCBIN(vde_h264, PATH_H264_INCBIN);
INCBIN(vde_jpeg, PATH_JPEG_INCBIN);

#define THREAD_PRIORITY   5
#define THREAD_STACK_SIZE 10240
#define THREAD_TIMESLICE  5
#define DEF_LAYER_NAME    "lcd"

#define DEF_ROW     1
#define DEF_COLUM   DEF_ROW
#define DEF_MAX_DECODING_INSTANCE   1

#define DEF_H264D_ONLY     0
#if defined(DEF_H264D_ONLY) && (DEF_H264D_ONLY==1)
    #define USE_PP      1
#else
    //#define USE_PP      0 // For rendering JPEG decoding directly
    #define USE_PP      1
#endif

typedef struct
{
    int i32RowNum;
    int i32ColumNum;
    int i32Index;
    int Reserved;
} S_VDE_VIWE_PARAM;


static int index = 0;
static uint32_t fbsize, fbaddr, fbpicese;
static rt_device_t psDevLcd = RT_NULL;
static struct rt_device_graphic_info s_GfxInfo;

static int vde_save_file(const void *data, size_t size)
{
    int fd;
    int wrote_size = 0;
    char filepath[64];

    if (!size || !data)
        return -1;

    rt_snprintf(filepath, sizeof(filepath), "/mnt/sd0/out/%04d.nv12", index);

    fd = open(filepath, O_WRONLY | O_CREAT);
    if (fd < 0)
    {
        LOG_E("Could not open %s for writing.", filepath);
        goto exit_ccap_save_file;
    }

    if ((wrote_size = write(fd, data, size)) != size)
    {
        LOG_E("Could not write to %s (%d != %d).", filepath, wrote_size, size);
        goto exit_ccap_save_file;
    }

    wrote_size = size;

    index++;

exit_ccap_save_file:

    if (fd >= 0)
        close(fd);

    return wrote_size;
}

static void pic_flush(VDE_FLUSH_CXT *psVDEFlushCtx)
{
    RT_ASSERT(psVDEFlushCtx != RT_NULL);

    if (psDevLcd && psVDEFlushCtx->u32FrameBufAddr && psVDEFlushCtx->u32FrameBufSize)
    {
        rt_device_control(psDevLcd, RTGRAPHIC_CTRL_PAN_DISPLAY, (void *)psVDEFlushCtx->u32FrameBufAddr);
    }
}

static void vde_decoder(void *parameter)
{
    int ret;
    uint8_t   *file_ptr;
    int   handle = -1;
    S_VDE_VIWE_PARAM sVDEViewParam = *((S_VDE_VIWE_PARAM *)parameter);

    if (parameter)
    {
        rt_free(parameter);
    }

#if USE_PP
    struct pp_params pp = {0};
    pp.frame_buf_w = s_GfxInfo.width;
    pp.frame_buf_h = s_GfxInfo.height;
    pp.img_out_x = (s_GfxInfo.width / sVDEViewParam.i32RowNum) * (sVDEViewParam.i32Index % sVDEViewParam.i32RowNum);
    pp.img_out_y = (s_GfxInfo.height / sVDEViewParam.i32ColumNum) * (sVDEViewParam.i32Index / sVDEViewParam.i32ColumNum);
    pp.img_out_w = s_GfxInfo.width / sVDEViewParam.i32RowNum;
    pp.img_out_h = s_GfxInfo.height / sVDEViewParam.i32ColumNum;
    pp.img_out_fmt = VC8000_PP_F_NV12;

    pp.rotation = VC8000_PP_ROTATION_NONE;
    pp.pp_out_dst = fbaddr;
    pp.pp_out_dst_bufsize = fbsize * fbpicese;

    LOG_I("[%dx%d #%d] %d %d %d %d out:%08x",
          sVDEViewParam.i32RowNum,
          sVDEViewParam.i32ColumNum,
          sVDEViewParam.i32Index,
          pp.img_out_x, pp.img_out_y, pp.img_out_w, pp.img_out_h, pp.pp_out_dst);
#endif

    LOG_I("START decoding.");

    //while (1)
    {
        uint32_t total_len, counter;
        uint32_t bs_len, remain;

        if (DEF_H264D_ONLY || sVDEViewParam.i32Index & 0x1)
        {
            handle = VC8000_Open(VC8000_CODEC_H264, pic_flush);
        }
        else
        {
            handle = VC8000_Open(VC8000_CODEC_JPEG, pic_flush);
        }

        if (handle < 0)
        {
            LOG_W("VC8000_Open failed! (%d)", handle);
            goto exit_vde_decoder;
        }

#if USE_PP
        ret = VC8000_PostProcess(handle, &pp);
        if (ret < 0)
        {
            LOG_E("VC8000_PostProcess failed! (%d)\n", ret);
            goto exit_vde_decoder;
        }
#endif

        if (DEF_H264D_ONLY || sVDEViewParam.i32Index & 0x1)
        {
            file_ptr = (uint8_t *)&incbin_vde_h264_start;
            bs_len = (uint32_t)((char *)&incbin_vde_h264_end - (char *)&incbin_vde_h264_start);
        }
        else
        {
            file_ptr = (uint8_t *)&incbin_vde_jpeg_start;
            bs_len = (uint32_t)((char *)&incbin_vde_jpeg_end - (char *)&incbin_vde_jpeg_start);
        }

        total_len = bs_len;
        counter = 0;

        do
        {
            ret = VC8000_Decode(handle, file_ptr, bs_len, (USE_PP ? RT_NULL : (uint8_t *)fbaddr), &remain);
            if (ret != 0)
            {
                LOG_E("VC8000_Decode error: %d\n", ret);
                goto exit_vde_decoder;
            }

            file_ptr += (bs_len - remain);
            bs_len = remain;
            counter++;

            LOG_I("[%d-%d]. (%d/%d) %d ", handle, counter, bs_len, total_len, remain);

            //vde_save_file(fbaddr, pp.frame_buf_w * pp.frame_buf_h * 3 / 2);
        }
        while (remain > 0);

        LOG_I("%s(%d) decode done.", (DEF_H264D_ONLY || (sVDEViewParam.i32Index & 0x1)) ? "H264" : "JPEG", handle);

        if (handle >= 0)
        {
            VC8000_Close(handle);
            handle = -1;
        }

    } //while(1)

exit_vde_decoder:

    if (handle >= 0)
    {
        VC8000_Close(handle);
        handle = -1;
    }

    if (DEF_H264D_ONLY != 1)  // JPEG
        rt_thread_mdelay(10000);

    if (psDevLcd)
        rt_device_close(psDevLcd);

    return;
}

static int vde_demo(void)
{
    int idx;
    S_VDE_VIWE_PARAM *psVDEViewParam;
    rt_err_t ret;

    psDevLcd = rt_device_find(DEF_LAYER_NAME);
    if (psDevLcd == RT_NULL)
    {
        LOG_E("Can't find %s", DEF_LAYER_NAME);
        return -1;
    }

    int pixfmt = RTGRAPHIC_PIXEL_FORMAT_NV12;

    if (rt_device_control(psDevLcd, RTGRAPHIC_CTRL_SET_MODE, &pixfmt) != RT_EOK)
    {
        LOG_E("Can't get LCD info %s\n", DEF_LAYER_NAME);
        return -1;
    }

    /* Get LCD Info */
    ret = rt_device_control(psDevLcd, RTGRAPHIC_CTRL_GET_INFO, &s_GfxInfo);
    if (ret != RT_EOK)
    {
        LOG_E("Can't get LCD info %s", DEF_LAYER_NAME);
        return -1;
    }

    LOG_I("LCD Width: %d",   s_GfxInfo.width);
    LOG_I("LCD Height: %d",  s_GfxInfo.height);
    LOG_I("LCD bpp:%d",   s_GfxInfo.bits_per_pixel);
    LOG_I("LCD pixel format:%d",   s_GfxInfo.pixel_format);
    LOG_I("LCD frame buffer@0x%08x",   s_GfxInfo.framebuffer);
    LOG_I("LCD frame buffer size:%d",   s_GfxInfo.smem_len);

    fbsize = (s_GfxInfo.width * s_GfxInfo.height * s_GfxInfo.bits_per_pixel / 8);
    fbpicese = s_GfxInfo.smem_len / fbsize;
    fbaddr = (uint32_t)s_GfxInfo.framebuffer;

    /* open lcd */
    ret = rt_device_open(psDevLcd, 0);
    if (ret != RT_EOK)
    {
        LOG_E("Can't open %s", DEF_LAYER_NAME);
        return -1;
    }

    for (idx = 0; idx < DEF_MAX_DECODING_INSTANCE; idx++)
    {
        psVDEViewParam = rt_malloc(sizeof(S_VDE_VIWE_PARAM));
        if (psVDEViewParam == RT_NULL)
        {
            rt_kprintf("Failed to allocate memory.\n");
            continue;
        }

        psVDEViewParam->i32RowNum = DEF_ROW;
        psVDEViewParam->i32ColumNum = DEF_COLUM;
        psVDEViewParam->i32Index = idx;
        psVDEViewParam->Reserved = 0 ;

        rt_thread_t vde_thread = rt_thread_create("vdedemo",
                                 vde_decoder,
                                 psVDEViewParam,
                                 THREAD_STACK_SIZE,
                                 THREAD_PRIORITY,
                                 THREAD_TIMESLICE);

        // rt_thread_control(vde_thread, RT_THREAD_CTRL_BIND_CPU, (void *)1);

        if (vde_thread != RT_NULL)
            rt_thread_startup(vde_thread);
    }

    return 0;
}

MSH_CMD_EXPORT(vde_demo, vde player);
//INIT_APP_EXPORT(vde_demo);

#endif

#endif

