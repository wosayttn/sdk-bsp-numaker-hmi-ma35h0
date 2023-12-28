/**************************************************************************//**
*
* @copyright (C) 2019 Nuvoton Technology Corp. All rights reserved.
*
* SPDX-License-Identifier: Apache-2.0
*
* Change Logs:
* Date            Author       Notes
* 2023-11-21      Wayne        First version
*
******************************************************************************/

#include <rtthread.h>
#include <rtdevice.h>
#include "drv_gpio.h"
#include "drv_sys.h"

#include "board.h"

#if defined(BOARD_USING_STORAGE_SPIFLASH)
#if defined(RT_USING_SFUD)
    #include "spi_flash.h"
    #include "spi_flash_sfud.h"
#endif

#include "drv_qspi.h"

#define W25X_REG_READSTATUS    (0x05)
#define W25X_REG_READSTATUS2   (0x35)
#define W25X_REG_WRITEENABLE   (0x06)
#define W25X_REG_WRITESTATUS   (0x01)
#define W25X_REG_QUADENABLE    (0x02)

static rt_uint8_t SpiFlash_ReadStatusReg(struct rt_qspi_device *qspi_device)
{
    rt_uint8_t u8Val;
    rt_err_t result = RT_EOK;
    rt_uint8_t w25x_txCMD1 = W25X_REG_READSTATUS;

    result = rt_qspi_send_then_recv(qspi_device, &w25x_txCMD1, 1, &u8Val, 1);
    RT_ASSERT(result > 0);

    return u8Val;
}

static rt_uint8_t SpiFlash_ReadStatusReg2(struct rt_qspi_device *qspi_device)
{
    rt_uint8_t u8Val;
    rt_err_t result = RT_EOK;
    rt_uint8_t w25x_txCMD1 = W25X_REG_READSTATUS2;

    result = rt_qspi_send_then_recv(qspi_device, &w25x_txCMD1, 1, &u8Val, 1);
    RT_ASSERT(result > 0);

    return u8Val;
}

static rt_err_t SpiFlash_WriteStatusReg(struct rt_qspi_device *qspi_device, uint8_t u8Value1, uint8_t u8Value2)
{
    rt_uint8_t w25x_txCMD1;
    rt_uint8_t au8Val[2];
    rt_err_t result;
    struct rt_qspi_message qspi_message = {0};

    /* Enable WE */
    w25x_txCMD1 = W25X_REG_WRITEENABLE;
    result = rt_qspi_send(qspi_device, &w25x_txCMD1, sizeof(w25x_txCMD1));
    if (result != sizeof(w25x_txCMD1))
        goto exit_SpiFlash_WriteStatusReg;

    /* Prepare status-1, 2 data */
    au8Val[0] = u8Value1;
    au8Val[1] = u8Value2;

    /* 1-bit mode: Instruction+payload */
    qspi_message.instruction.content = W25X_REG_WRITESTATUS;
    qspi_message.instruction.qspi_lines = 1;

    qspi_message.qspi_data_lines   = 1;
    qspi_message.parent.cs_take    = 1;
    qspi_message.parent.cs_release = 1;
    qspi_message.parent.send_buf   = &au8Val[0];
    qspi_message.parent.length     = sizeof(au8Val);
    qspi_message.parent.next       = RT_NULL;

    if (rt_qspi_transfer_message(qspi_device, &qspi_message) != sizeof(au8Val))
    {
        result = -RT_ERROR;
    }

    result  = RT_EOK;

exit_SpiFlash_WriteStatusReg:

    return result;
}

static void SpiFlash_WaitReady(struct rt_qspi_device *qspi_device)
{
    volatile uint8_t u8ReturnValue;

    do
    {
        u8ReturnValue = SpiFlash_ReadStatusReg(qspi_device);
        u8ReturnValue = u8ReturnValue & 1;
    }
    while (u8ReturnValue != 0);   // check the BUSY bit
}

static void SpiFlash_EnterQspiMode(struct rt_qspi_device *qspi_device)
{
    rt_err_t result = RT_EOK;

    uint8_t u8Status1 = SpiFlash_ReadStatusReg(qspi_device);
    uint8_t u8Status2 = SpiFlash_ReadStatusReg2(qspi_device);

    u8Status2 |= W25X_REG_QUADENABLE;

    result = SpiFlash_WriteStatusReg(qspi_device, u8Status1, u8Status2);
    RT_ASSERT(result == RT_EOK);

    SpiFlash_WaitReady(qspi_device);
}

static void SpiFlash_ExitQspiMode(struct rt_qspi_device *qspi_device)
{
    rt_err_t result = RT_EOK;
    uint8_t u8Status1 = SpiFlash_ReadStatusReg(qspi_device);
    uint8_t u8Status2 = SpiFlash_ReadStatusReg2(qspi_device);

    u8Status2 &= ~W25X_REG_QUADENABLE;

    result = SpiFlash_WriteStatusReg(qspi_device, u8Status1, u8Status2);
    RT_ASSERT(result == RT_EOK);

    SpiFlash_WaitReady(qspi_device);
}

static int rt_hw_spiflash_init(void)
{
    if (nu_qspi_bus_attach_device("qspi0", "qspi01", 4, SpiFlash_EnterQspiMode, SpiFlash_ExitQspiMode) != RT_EOK)
        return -1;

#if defined(RT_USING_SFUD)
    if (rt_sfud_flash_probe(FAL_USING_NOR_FLASH_DEV_NAME, "qspi01") == RT_NULL)
    {
        return -(RT_ERROR);
    }
#endif
    return 0;
}
INIT_DEVICE_EXPORT(rt_hw_spiflash_init);
#endif /* BOARD_USING_STORAGE_SPIFLASH */

#if defined(BOARD_USING_STORAGE_SPINAND) && defined(NU_PKG_USING_SPINAND)

#include "drv_qspi.h"
#include "spinand.h"

struct rt_mtd_nand_device mtd_partitions[MTD_SPINAND_PARTITION_NUM] =
{
    [0] =
    {
        /*nand0: U-boot, env, rtthread*/
        .block_start = 0,
        .block_end   = 63,
        .block_total = 64,
    },
    [1] =
    {
        /*nand1: for filesystem mounting*/
        .block_start = 64,
        .block_end   = 4095,
        .block_total = 4032,
    },
    [2] =
    {
        /*nand2: Whole blocks size, overlay*/
        .block_start = 0,
        .block_end   = 4095,
        .block_total = 4096,
    }
};

static int rt_hw_spinand_init(void)
{
    if (nu_qspi_bus_attach_device("qspi0", "qspi01", 4, RT_NULL, RT_NULL) != RT_EOK)
        return -1;

    if (rt_hw_mtd_spinand_register("qspi01") != RT_EOK)
        return -1;

    return 0;
}

INIT_DEVICE_EXPORT(rt_hw_spinand_init);
#endif

#if defined(BOARD_USING_STORAGE_RAWNAND) && defined(BSP_USING_NFI)
struct rt_mtd_nand_device mtd_partitions_nfi[MTD_NFI_PARTITION_NUM] =
{
    [0] =
    {
        /*nand0:  rtthread*/
        .block_start = 0,
        .block_end   = 63,
        .block_total = 64,
    },
    [1] =
    {
        /*nand1: for filesystem mounting*/
        .block_start = 64,
        .block_end   = 8191,
        .block_total = 8128,
    },
    [2] =
    {
        /*nand2: Whole blocks size, overlay*/
        .block_start = 0,
        .block_end   = 8191,
        .block_total = 8192,
    }
};
#endif

#if defined(BOARD_USING_NAU8822) && defined(NU_PKG_USING_NAU8822)
#include <acodec_nau8822.h>
S_NU_NAU8822_CONFIG sCodecConfig =
{
    .i2c_bus_name = "i2c0",

    .i2s_bus_name = "sound0",

    .pin_phonejack_en = NU_GET_PININDEX(NU_PM, 6),

    .pin_phonejack_det = NU_GET_PININDEX(NU_PM, 7),
};

int rt_hw_nau8822_port(void)
{
    if (nu_hw_nau8822_init(&sCodecConfig) != RT_EOK)
        return -1;

    return 0;
}
INIT_COMPONENT_EXPORT(rt_hw_nau8822_port);
#endif /* BOARD_USING_NAU8822 */

#if defined(NU_PKG_USING_ADC_TOUCH)
#include "adc_touch.h"
S_CALIBRATION_MATRIX g_sCalMat = { -17558, 1, 69298832, -10, 11142, -2549195, 65536 };
#endif

#if defined(BOARD_USING_LCM)

#if defined(PKG_USING_GUIENGINE)
    #include <rtgui/driver.h>
#endif

#include <drv_gpio.h>

/* defined the LCM_BLEN pin: PK7 */
#define LCM_BACKLIGHT_CTRL  NU_GET_PININDEX(NU_PA, 15)
#define LCM_RST             NU_GET_PININDEX(NU_PF, 1)

#define EPWM_DEV_NAME       "epwm1"
#define LCM_PWM_CHANNEL      (3)

void nu_lcd_backlight_on(void)
{
#if defined(BOARD_USING_LCM_FW070TFT_WSVGA)
    struct rt_device_pwm *pwm_dev;

    if ((pwm_dev = (struct rt_device_pwm *)rt_device_find(EPWM_DEV_NAME)) != RT_NULL)
    {
        rt_pwm_enable(pwm_dev, LCM_PWM_CHANNEL);
        rt_pwm_set(pwm_dev, LCM_PWM_CHANNEL, 100000, 100);
    }
    else
    {
        rt_kprintf("Can't find %s\n", EPWM_DEV_NAME);
    }
#endif

    rt_pin_mode(LCM_BACKLIGHT_CTRL, PIN_MODE_OUTPUT);
    rt_pin_write(LCM_BACKLIGHT_CTRL, PIN_HIGH);
}

void nu_lcd_backlight_off(void)
{
#if defined(BOARD_USING_LCM_FW070TFT_WSVGA)
    struct rt_device_pwm *pwm_dev;

    if ((pwm_dev = (struct rt_device_pwm *)rt_device_find(EPWM_DEV_NAME)) != RT_NULL)
    {
        rt_pwm_disable(pwm_dev, LCM_PWM_CHANNEL);
    }
    else
    {
        rt_kprintf("Can't find %s\n", EPWM_DEV_NAME);
    }
#endif

    rt_pin_mode(LCM_BACKLIGHT_CTRL, PIN_MODE_OUTPUT);
    rt_pin_write(LCM_BACKLIGHT_CTRL, PIN_LOW);
}

int rt_hw_lcm_port(void)
{
#if defined(PKG_USING_GUIENGINE)
    rt_device_t lcm_vpost;
    lcm_vpost = rt_device_find("lcd");
    if (lcm_vpost)
    {
        rtgui_graphic_set_device(lcm_vpost);
    }
#endif

    rt_pin_mode(LCM_RST, PIN_MODE_OUTPUT);
    rt_pin_write(LCM_RST, PIN_HIGH);

    return 0;
}
INIT_COMPONENT_EXPORT(rt_hw_lcm_port);
#endif /* BOARD_USING_LCM */

