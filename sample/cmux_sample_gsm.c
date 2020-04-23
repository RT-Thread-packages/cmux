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
#include <rtthread.h>

#define CMUX_PPP_NAME "cmux_ppp"
#define CMUX_PPP_PORT 1

#define CMUX_AT_NAME "cmux_at"
#define CMUX_AT_PORT 2

#define DBG_TAG "cmux.sample"

#ifdef CMUX_DEBUG
#define DBG_LVL DBG_LOG
#else
#define DBG_LVL DBG_INFO
#endif

#include <rtdbg.h>

struct cmux *sample = RT_NULL;
int cmux_sample(void)
{
    rt_err_t result;

    /* find cmux object through the actual serial name < the actual serial has been related in the cmux.c file > */
    sample = cmux_object_find(CMUX_DEPEND_NAME);
    if (sample == RT_NULL)
    {
        result = -RT_ERROR;
        LOG_E("Can't find %s", CMUX_DEPEND_NAME);
        goto end;
    }
    /* startup the cmux receive thread, attach control chananel and open it */
    result = cmux_start(sample);
    if (result != RT_EOK)
    {
        LOG_E("cmux sample start error. Can't find %s", CMUX_DEPEND_NAME);
        goto end;
    }
    LOG_I("cmux sample (%s) start successful.", CMUX_DEPEND_NAME);

    /* attach AT function into cmux */
    result = cmux_attach(sample, CMUX_AT_PORT, CMUX_AT_NAME, RT_DEVICE_FLAG_DMA_RX, RT_NULL);
    if (result != RT_EOK)
    {
        LOG_E("cmux attach (%s) failed.", CMUX_AT_NAME);
        goto end;
    }
    LOG_I("cmux object channel (%s) attach successful.", CMUX_AT_NAME);

    /* attach PPP function into cmux */
    result = cmux_attach(sample, CMUX_PPP_PORT, CMUX_PPP_NAME, RT_DEVICE_FLAG_DMA_RX, RT_NULL);
    if (result != RT_EOK)
    {
        LOG_E("cmux attach %s failed.", CMUX_PPP_NAME);
        goto end;
    }
    LOG_I("cmux object channel (%s) attach successful.", CMUX_PPP_NAME);
end:
    return RT_EOK;
}
#ifdef CMUX_ATUO_INITIZATION
INIT_APP_EXPORT(cmux_sample);
#endif
MSH_CMD_EXPORT_ALIAS(cmux_sample, cmux_start, a sample of cmux function);
