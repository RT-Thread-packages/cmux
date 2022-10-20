/*
 * Copyright (c) 2006-2020, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author         Notes
 * 2020-04-15    xiangxistu      the first version
 */

#include <cmux.h>

#ifdef PKG_USING_PPP_DEVICE
#include <ppp_chat.h>
#else
#include <cmux_chat.h>
#endif

#define DBG_TAG    "cmux.gsm"
#ifdef CMUX_DEBUG
#define DBG_LVL   DBG_LOG
#else
#define DBG_LVL   DBG_INFO
#endif
#include <rtdbg.h>

/**
 * +CMUX=<mode>[,<subset>[,<port_speed>[,<N1>[,<T1>[,<N2>[,<T2>[,<T3>[,<k>]]]]]]]]
 * port_speed:
 * 1 9600
   2 19200
   3 38400
   4 57600
   5 115200
   6 230400
   7 460800
   8 921600
 *
 */
#ifndef CMUX_CMD
#define CMUX_CMD "AT+CMUX=0,0,5,2048,20,3,30,10,2"
#endif

static struct cmux *gsm = RT_NULL;
static char cmux_cmd[64] = { CMUX_CMD };

static struct modem_chat_data cmd[] =
{
    {"AT",              MODEM_CHAT_RESP_OK,              10, 1, RT_FALSE},
    {cmux_cmd,          MODEM_CHAT_RESP_OK,               5, 1, RT_FALSE},
};

/**
 * configuration the AT+CMUX command parameter
 *
 * default @see CMUX_CMD
 *
 * @param mode 0 Basic option, 1 Advanced option
 * @param subset 0 UIH frames used only, 1 UI frames used only, 2 I frames used only
 * @param port_speed serial port speed, like 115200
 * @param N1 maximum frame size
 * @param T1 acknowledgement timer in units of ten milliseconds
 * @param N2 maximum number of re-transmissions
 * @param T2 response timer for the multiplexer control channel in units of ten milliseconds, T2 must be longer than T1
 * @param T3 wake up response timer in seconds
 * @param k window size, for Advanced operation with Error Recovery options
 */
void cmux_at_cmd_cfg(uint8_t mode, uint8_t subset, uint32_t port_speed, uint32_t N1, uint32_t T1, uint32_t N2,
        uint32_t T2, uint32_t T3, uint32_t k)
{
    RT_ASSERT(T2 > T1);
    switch(port_speed)
    {
        case 9600: port_speed = 1; break;
        case 19200: port_speed = 2; break;
        case 38400: port_speed = 3; break;
        case 57600: port_speed = 4; break;
        case 115200: port_speed = 5; break;
        case 230400: port_speed = 6; break;
        case 460800: port_speed = 7; break;
        case 921600: port_speed = 8; break;
        default: RT_ASSERT("Not support port speed" && 0);
    }

    rt_snprintf(cmux_cmd, sizeof(cmux_cmd), "AT+CMUX=%d,%d,%d,%d,%d,%d,%d,%d,%d", mode, subset, port_speed, N1, T1, N2, T2,
            T3, k);
}

static rt_err_t cmux_at_command(struct rt_device *device)
{
    /* private control, you can add power control */

//    rt_thread_mdelay(5000);
    return modem_chat(device, cmd, sizeof(cmd) / sizeof(cmd[0]));
}

static rt_err_t cmux_gsm_start(struct cmux *obj)
{
    rt_err_t result = 0;
    struct rt_device *device = RT_NULL;

    device = obj->dev;
    /* using DMA mode first */
    result = rt_device_open(device, RT_DEVICE_OFLAG_RDWR | RT_DEVICE_FLAG_DMA_RX);
    /* result interrupt mode when DMA mode not supported */
    if (result == -RT_EIO)
    {
        result = rt_device_open(device, RT_DEVICE_OFLAG_RDWR | RT_DEVICE_FLAG_INT_RX);
    }
    if(result != RT_EOK)
    {
        LOG_E("cmux can't open %s.", device->parent.name);
        goto _end;
    }
    LOG_I("cmux has been control %s.", device->parent.name);

    result = cmux_at_command(device);
    if(result != RT_EOK)
    {
        LOG_E("cmux start failed.");
        goto _end;
    }

_end:
    return result;
}
const struct cmux_ops cmux_ops =
{
    cmux_gsm_start,
    RT_NULL,
    RT_NULL
};

int cmux_gsm_init(void)
{
    gsm = rt_malloc(sizeof(struct cmux));
    rt_memset(gsm, 0, sizeof(struct cmux));

    gsm->ops = &cmux_ops;

    return cmux_init(gsm, CMUX_DEPEND_NAME, CMUX_PORT_NUMBER, RT_NULL);
}
INIT_COMPONENT_EXPORT(cmux_gsm_init);
