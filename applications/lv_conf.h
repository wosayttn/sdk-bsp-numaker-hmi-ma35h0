/*
 * Copyright (c) 2006-2022, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author        Notes
 * 2022-2-17      Wayne         First version
 */

#ifndef LV_CONF_H
#define LV_CONF_H

#include "rtconfig.h"

//#define NU_PKG_LVGL_RENDERING_LAYER    "overlay"
//#define NU_OVERLAY_COLOR_KEY           0xFFFFFFFF
#define NU_PKG_LVGL_RENDERING_FULL_REFRESH     0

#define LV_USE_DRAW_SW                  1
#define LV_USE_DRAW_SW_ASM              LV_DRAW_SW_ASM_NEON
#define LV_DRAW_SW_DRAW_UNIT_CNT        2
#define LV_DRAW_BUF_ALIGN               64
#define LV_DRAW_BUF_STRIDE_ALIGN        4
#define LV_DEF_REFR_PERIOD              PKG_LVGL_DISP_REFR_PERIOD

#define LV_COLOR_DEPTH                  BSP_LCD_BPP
#define LV_HOR_RES_MAX                  BSP_LCD_WIDTH
#define LV_VER_RES_MAX                  BSP_LCD_HEIGHT

#define LV_FONT_MONTSERRAT_12           1
#define LV_FONT_MONTSERRAT_14           1
#define LV_FONT_MONTSERRAT_16           1
#define LV_FONT_MONTSERRAT_18           1
#define LV_FONT_MONTSERRAT_20           1
#define LV_FONT_MONTSERRAT_22           1
#define LV_FONT_MONTSERRAT_24           1
#define LV_FONT_MONTSERRAT_26           1
#define LV_FONT_MONTSERRAT_28           1
#define LV_FONT_MONTSERRAT_30           1
#define LV_FONT_MONTSERRAT_32           1
#define LV_FONT_MONTSERRAT_34           1
#define LV_FONT_MONTSERRAT_36           1
#define LV_FONT_MONTSERRAT_38           1
#define LV_FONT_MONTSERRAT_40           1
#define LV_FONT_MONTSERRAT_42           1
#define LV_FONT_MONTSERRAT_44           1
#define LV_FONT_MONTSERRAT_46           1
#define LV_FONT_MONTSERRAT_48           1

#define CONFIG_LV_MEM_SIZE              (1024*1024)
#define CONFIG_LV_CACHE_DEF_SIZE        (1024*1024)

/* Please comment LV_USE_DEMO_MUSIC declaration before un-comment below */
#define LV_USE_DEMO_WIDGETS             1
//#define LV_USE_DEMO_BENCHMARK           1
//#define LV_USE_DEMO_MUSIC             1
#if LV_USE_DEMO_MUSIC
    #define LV_DEMO_MUSIC_AUTO_PLAY     1
#endif

#define LV_USE_SYSMON                   1
#define LV_USE_PERF_MONITOR             1
#define LV_USE_LOG                      0

#if LV_USE_LOG == 1
    //#define LV_LOG_LEVEL                    LV_LOG_LEVEL_TRACE
    #define LV_LOG_LEVEL                    LV_LOG_LEVEL_INFO
    //#define LV_LOG_LEVEL                    LV_LOG_LEVEL_WARN
    //#define LV_LOG_LEVEL                    LV_LOG_LEVEL_ERROR
    //#define LV_LOG_LEVEL                    LV_LOG_LEVEL_USER
    //#define LV_LOG_LEVEL                    LV_LOG_LEVEL_NONE
#endif

#endif
