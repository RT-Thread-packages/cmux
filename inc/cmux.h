/*
 * Copyright (c) 2006-2020, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author         Notes
 * 2020-04-15    xiangxistu      the first version
 */

#ifndef __CMUX_H__
#define __CMUX_H__

#include <rtdef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CMUX_BUFFER_SIZE   2048

#define CMUX_SW_VERSION           "1.0.0"
#define CMUX_SW_VERSION_NUM       0x10000

struct cmux_buffer
{
    rt_uint8_t data[CMUX_BUFFER_SIZE];                    /* cmux data buffer */
    rt_uint8_t *read_point;
    rt_uint8_t *write_point;
    rt_uint8_t *end_point;
    int flag_found;                                       /* the flag whether you find cmux frame */
};

struct cmux_frame
{
    rt_uint8_t channel;                                   /* the frame channel */
    rt_uint8_t control;                                   /* the type of frame */
    int data_length;                                      /* frame length */
    rt_uint8_t *data;                                     /* the point for cmux data */
};

struct frame
{
    struct cmux_frame *frame;                             /* the point for cmux frame */

    rt_slist_t frame_list;                                /* slist for different virtual serial */
};

struct cmux_vcoms
{
    struct rt_device device;                              /* virtual device */

    rt_slist_t flist;                                     /* head of frame_list */

    rt_uint16_t frame_index;                              /* the length of flist */

    rt_uint8_t link_port;                                 /* link port id */

    rt_bool_t frame_using_status;                         /* This is designed for long frame when we read data; the flag will be "1" when long frame haven't reading done */

    struct cmux_frame *frame;

    rt_size_t length;

    rt_uint8_t *data;
};

struct cmux
{
    struct rt_device *dev;                                /* device object */
    const struct cmux_ops *ops;                           /* cmux device ops interface */
    struct cmux_buffer *buffer;                           /* cmux buffer */
    struct cmux_frame *frame;                             /* cmux frame point */
    rt_thread_t recv_tid;                                 /* receive thread point */
    rt_uint8_t vcom_num;                                  /* the cmux port number */
    struct cmux_vcoms *vcoms;                             /* array */

    struct rt_event *event;                               /* internal communication */

    rt_slist_t list;                                      /* cmux list */

    void *user_data;                                      /* reserve */
};

struct cmux_ops
{
    rt_err_t  (*start)     (struct cmux *obj);
    rt_err_t  (*stop)      (struct cmux *obj);
    rt_err_t  (*control)   (struct cmux *obj, int cmd, void *arg);
};



/* those function is opening for external file */
rt_err_t cmux_init(struct cmux *object, const char *dev_name, rt_uint8_t vcom_num, void *user_data);
rt_err_t cmux_start(struct cmux *object);
rt_err_t cmux_stop(struct cmux *object);
rt_err_t cmux_attach(struct cmux *object, int port, const char *alias_name, rt_uint16_t flags, void *user_data);
rt_err_t cmux_detach(struct cmux *object, const char *alias_name);

/* cmux_utils */
rt_uint8_t cmux_frame_check(const rt_uint8_t *input, int length);
struct cmux *cmux_object_find(const char *name);

#ifdef  __cplusplus
    }
#endif

#endif  /* __CMUX_H__ */
