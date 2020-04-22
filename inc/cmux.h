/*
 * Copyright (c) 2006-2020, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author         Notes
 * 2020-04-15    xiangxistu      the first version
 */

#include <rtdef.h>

#define CMUX_BUFFER_SIZE   2048

#ifdef RT_USING_LIBC
#include <lib.c>
#else
#define min(a,b) ((a)<=(b)?(a):(b))
#endif

// increases buffer pointer by one and wraps around if necessary
#define INC_BUF_POINTER(buf,p) p++; if (p == buf->end_point) p = buf->data;

/* Tells, how many chars are saved into the buffer.
 *
 */
//int cmux_buffer_length(cmux_buffer *buf);
#define cmux_buffer_length(buff) ((buff->read_point > buff->write_point) ? (CMUX_BUFFER_SIZE - (buff->read_point - buff->write_point)) : (buff->write_point - buff->read_point))

/* Tells, how much free space there is in the buffer
 */
//int cmux_buffer_free(cmux_buffer *buf);
#define cmux_buffer_free(buff) ((buff->read_point > buff->write_point) ? (buff->read_point - buff->write_point) : (CMUX_BUFFER_SIZE - (buff->write_point - buff->read_point)))

typedef struct cmux_buffer
{
  rt_uint8_t data[CMUX_BUFFER_SIZE];                        /* cmux data buffer */
  rt_uint8_t *read_point;
  rt_uint8_t *write_point;
  rt_uint8_t *end_point;
  int flag_found;                                          /* the flag whether you find cmux frame */
} cmux_buffer;

typedef struct cmux_frame
{
    rt_uint8_t channel;                                   /* the frame channel */
    rt_uint8_t control;                                   /* the type of frame */
    int data_length;                                      /* frame length */
    rt_uint8_t *data;                                     /* the point for cmux data */
} cmux_frame;

typedef struct frame
{
    struct cmux_frame *frame;                             /* the point for cmux frame */

    rt_slist_t frame_list;                                /* slist for different virtual serial */
}frame;

typedef struct cmux_vcoms
{
    struct rt_device device;                              /* virtual device */

    rt_slist_t flist;                                     /* head of frame_list */

    rt_uint16_t frame_index;                              /* the length of flist */

    rt_bool_t frame_using_status;                         /* This is designed for long frame when we read data; the flag will be "1" when long frame haven't reading done */
}cmux_vcoms;

typedef struct cmux
{
    struct rt_device *dev;                                /* device object */
    const struct cmux_ops *ops;                           /* cmux device ops interface */
    struct cmux_buffer *buffer;                           /* cmux buffer */
    struct cmux_frame *frame;                             /* cmux frame point */
    rt_thread_t recv_tid;                                 /* receive thread point */
    rt_uint8_t vcom_num;                                  /* the cmux port number */
    struct cmux_vcoms *vcoms;                             /* array */

    struct rt_event *event;                               /* internal communication */
    struct rt_messagequeue *mq;                           /* internal communication */

    rt_slist_t list;                                      /* cmux list */

    void *user_data;                                      /* reserve */
}cmux;

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
