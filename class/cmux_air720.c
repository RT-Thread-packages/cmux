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

#define DBG_TAG    "cmux.air720"

#ifdef CMUX_DEBUG
#define DBG_LVL   DBG_LOG
#else
#define DBG_LVL   DBG_INFO
#endif

#include <rtdbg.h>

struct cmux *air720 = RT_NULL;

static const struct modem_chat_data cmd[] =
{
    {"AT",              MODEM_CHAT_RESP_OK,              10, 1, RT_FALSE},
    {CMUX_CMD,          MODEM_CHAT_RESP_OK,               5, 1, RT_FALSE},
};

static rt_err_t cmux_at_command(struct rt_device *device)
{
    //private control

    return modem_chat(device, cmd, sizeof(cmd) / sizeof(cmd[0]));
}

rt_err_t air720_cmux_start(struct cmux *obj)
{
    rt_err_t result = 0;
    struct rt_device *device = RT_NULL;

    device = obj->dev;
    result = rt_device_open(device, RT_DEVICE_OFLAG_RDWR | RT_DEVICE_FLAG_INT_RX);
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
    air720_cmux_start,
    RT_NULL,
    RT_NULL
};

int cmux_air720_init(void)
{
    air720 = rt_malloc(sizeof(struct cmux));
    rt_memset(air720, 0, sizeof(struct cmux));
    
    air720->ops = &cmux_ops;

    return cmux_init(air720, CMUX_DEPEND_NAME, CMUX_PORT_NUMBER, RT_NULL);
}
INIT_COMPONENT_EXPORT(cmux_air720_init);
