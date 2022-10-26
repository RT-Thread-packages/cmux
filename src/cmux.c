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
#include <rthw.h>

// bits: Poll/final, Command/Response, Extension
#define CMUX_CONTROL_PF 16
#define CMUX_ADDRESS_CR 2
#define CMUX_ADDRESS_EA 1
// the types of the frames
#define CMUX_FRAME_SABM 47
#define CMUX_FRAME_UA 99
#define CMUX_FRAME_DM 15
#define CMUX_FRAME_DISC 67
#define CMUX_FRAME_UIH 239
#define CMUX_FRAME_UI 3
// the types of the control channel commands
#define CMUX_C_CLD 193
#define CMUX_C_TEST 33
#define CMUX_C_MSC 225
#define CMUX_C_NSC 17
// basic mode flag for frame start and end
#define CMUX_HEAD_FLAG (unsigned char)0xF9

#define CMUX_DHCL_MASK       63         /* DLCI number is port number, 63 is the mask of DLCI; C/R bit is 1 when we send data */
#define CMUX_DATA_MASK       127        /* when data length is out of 127( 0111 1111 ), we must use two bytes to describe data length in the cmux frame */
#define CMUX_HIGH_DATA_MASK  32640      /* 32640 (‭ 0111 1111 1000 0000 ‬), the mask of high data bits */

#define CMUX_COMMAND_IS(command, type) ((type & ~CMUX_ADDRESS_CR) == command)
#define CMUX_PF_ISSET(frame) ((frame->control & CMUX_CONTROL_PF) == CMUX_CONTROL_PF)
#define CMUX_FRAME_IS(type, frame) ((frame->control & ~CMUX_CONTROL_PF) == type)

#define min(a, b) ((a) <= (b) ? (a) : (b))

/* increases buffer pointer by one and wraps around if necessary */
#define INC_BUF_POINTER(buf, p)  \
    (p)++;                       \
    if ((p) == (buf)->end_point) \
        (p) = (buf)->data;

/* Tells, how many chars are saved into the buffer */
#define cmux_buffer_length(buff) (((buff)->read_point > (buff)->write_point) ? (CMUX_BUFFER_SIZE - ((buff)->read_point - (buff)->write_point)) : ((buff)->write_point - (buff)->read_point))

/* Tells, how much free space there is in the buffer */
#define cmux_buffer_free(buff) (((buff)->read_point > (buff)->write_point) ? ((buff)->read_point - (buff)->write_point) : (CMUX_BUFFER_SIZE - ((buff)->write_point - (buff)->read_point)))

#define CMUX_THREAD_STACK_SIZE (CMUX_RECV_READ_MAX + 1536)
#define CMUX_THREAD_PRIORITY 8

#define CMUX_RECIEVE_RESET 0
#define CMUX_RECIEVE_BEGIN 1
#define CMUX_RECIEVE_PROCESS 2
#define CMUX_RECIEVE_END 3

#define CMUX_EVENT_RX_NOTIFY 1 /* serial incoming a byte */
#define CMUX_EVENT_CHANNEL_OPEN 2
#define CMUX_EVENT_CHANNEL_CLOSE 4
#define CMUX_EVENT_CHANNEL_OPEN_REQ 8
#define CMUX_EVENT_CHANNEL_CLOSE_REQ 16
#define CMUX_EVENT_FUNCTION_EXIT 32

#define DBG_TAG "cmux"

#ifdef CMUX_DEBUG
#define DBG_LVL DBG_LOG
#else
#define DBG_LVL DBG_INFO
#endif
#include <rtdbg.h>

static rt_size_t cmux_send_data(struct rt_device *dev, int port, rt_uint8_t type, const char *data, int length);
static rt_slist_t cmux_list = RT_SLIST_OBJECT_INIT(cmux_list);
/* only one cmux object can be created */
static struct cmux *_g_cmux = RT_NULL;

/**
 * Get the cmux object that your used device.
 *
 * @param name       the name of your select device.
 *
 * @return  struct cmux object point
 */
struct cmux *cmux_object_find(const char *name)
{
    struct cmux *cmux = RT_NULL;
    struct rt_slist_node *node = RT_NULL;
    rt_base_t level;

    level = rt_hw_interrupt_disable();
    rt_slist_for_each(node, &cmux_list)
    {
        cmux = rt_slist_entry(node, struct cmux, list);
        if (rt_strncmp(cmux->dev->parent.name, name, RT_NAME_MAX) == 0)
        {
            rt_hw_interrupt_enable(level);
            return cmux;
        }
    };

    rt_hw_interrupt_enable(level);
    return RT_NULL;
}

/**
 * Receive callback function , send CMUX_EVENT_RX_NOTIFY event when uart acquire data
 *
 * @param dev       the point of device driver structure, uart structure
 * @param size      the indication callback function need this parameter
 *
 * @return  RT_EOK
 */
static rt_err_t cmux_rx_ind(rt_device_t dev, rt_size_t size)
{
    RT_ASSERT(dev != RT_NULL);
    struct cmux *cmux = RT_NULL;

    cmux = _g_cmux;

    /* when receive data from uart , send event to wake up receive thread */
    rt_event_send(cmux->event, CMUX_EVENT_RX_NOTIFY);

    return RT_EOK;
}

/**
 *  invoke callback function
 *
 * @param cmux          cmux object
 * @param port          the number of virtual channel
 * @param size          the size of we receive from cmux
 *
 * @return  the point of rt_device_write fucntion or RT_NULL
 */
void cmux_vcom_isr(struct cmux *cmux, rt_uint8_t port, rt_size_t size)
{
    /* when receive data, we should notify user immediately. */
    if (cmux->vcoms[port].device.rx_indicate != RT_NULL)
    {
        cmux->vcoms[port].device.rx_indicate(&cmux->vcoms[port].device, size);
    }
    else
    {
        LOG_W("channel[%02d] haven appended data, please set rx_indicate and clear receive data.", port);
    }
}

/**
 *  allocate buffer for cmux object receive
 *
 * @param RT_NULL
 *
 * @return  the point of struct cmux_buffer
 */
static struct cmux_buffer *cmux_buffer_init()
{
    struct cmux_buffer *buff = RT_NULL;
    buff = rt_malloc(sizeof(struct cmux_buffer));
    if (buff == RT_NULL)
    {
        return RT_NULL;
    }
    rt_memset(buff, 0, sizeof(struct cmux_buffer));
    buff->read_point = buff->data;
    buff->write_point = buff->data;
    buff->end_point = buff->data + CMUX_BUFFER_SIZE;
    return buff;
}

/**
 *  destroy buffer for cmux object receive
 *
 * @param frame         the point of cmux_frame
 *
 * @return  RT_NULL
 */
static void cmux_frame_destroy(struct cmux_frame *frame)
{
    if ((frame->data_length > 0) && frame->data)
    {
        rt_free(frame->data);
    }
    if (frame)
    {
        rt_free(frame);
    }
}

/**
 *  initial virtual serial for different channel, initial slist for channel
 *
 * @param cmux          cmux object
 * @param channel       the number of virtual serial
 *
 * @return  RT_NULL
 */
static void vcoms_cmux_frame_init(struct cmux *cmux, int channel)
{
    rt_base_t level;

    level = rt_hw_interrupt_disable();
    rt_slist_init(&cmux->vcoms[channel].flist);
    rt_hw_interrupt_enable(level);
    cmux->vcoms[channel].frame_index = 0;

    LOG_D("init cmux data channel(%d) list.", channel);
}

/**
 *  push cmux frame data into slist for different channel virtual serial
 *
 * @param cmux          cmux object
 * @param channel       the number of virtual serial
 * @param frame         the point of frame data
 *
 * @return  RT_EOK      successful
 *          RT_ENOMEM   allocate memory failed
 */
static rt_err_t cmux_frame_push(struct cmux *cmux, int channel, struct cmux_frame *frame)
{
    rt_base_t level;
    struct frame *frame_new = RT_NULL;
    rt_uint16_t frame_len = cmux->vcoms[channel].frame_index;

    if (frame_len <= CMUX_MAX_FRAME_LIST_LEN)
    {
        frame_new = rt_malloc(sizeof(struct frame));
        if (frame_new == RT_NULL)
        {
            LOG_E("can't malloc <struct frame> to record data address.");
            return -RT_ENOMEM;
        }
        else
        {
            rt_memset(frame_new, 0, sizeof(struct frame));

            frame_new->frame = frame;
            rt_slist_init(&frame_new->frame_list);
        }

        level = rt_hw_interrupt_disable();
        rt_slist_append(&cmux->vcoms[channel].flist, &frame_new->frame_list);
        rt_hw_interrupt_enable(level);

#ifdef CMUX_DEBUG
        LOG_HEX("CMUX_RX", 32, frame->data, frame->data_length);
#endif

        LOG_D("new message (len:%d) for channel (%d) is append, Message total: %d.", frame_new->frame->data_length, channel, ++cmux->vcoms[channel].frame_index);

        return RT_EOK;
    }
    LOG_E("malloc failed, the message for channel(%d) is long than CMUX_MAX_FRAME_LIST_LEN(%d).", channel, CMUX_MAX_FRAME_LIST_LEN);
    return -RT_ENOMEM;
}

/**
 *  pop cmux frame data from slist for different channel virtual serial
 *
 * @param cmux          cmux object
 * @param channel       the number of virtual serial
 *
 * @return  frame_data  successful
 *          RT_NULL     no message on the slist
 */
static struct cmux_frame *cmux_frame_pop(struct cmux *cmux, int channel)
{
    rt_base_t level;
    struct frame *frame = RT_NULL;
    struct cmux_frame *frame_data = RT_NULL;
    struct rt_slist_node *frame_list_find = RT_NULL;
    rt_slist_t *frame_list = RT_NULL;

    frame_list = &cmux->vcoms[channel].flist;

    frame_list_find = rt_slist_first(frame_list);
    if (frame_list_find != RT_NULL)
    {
        frame = rt_container_of(frame_list_find, struct frame, frame_list);
        frame_data = frame->frame;

        level = rt_hw_interrupt_disable();
        rt_slist_remove(frame_list, frame_list_find);
        rt_hw_interrupt_enable(level);

        LOG_D("A message (len:%d) for channel (%d) has been used, Message remain: %d.", frame_data->data_length, channel, --cmux->vcoms[channel].frame_index);
        rt_free(frame);
    }

    return frame_data;
}

/**
 *  write data into cmux buffer
 *
 * @param buff        the buffer of cmux object
 * @param input       the point of data
 * @param count       the length of data
 *
 * @return length     the length of write into cmux buffer
 *
 */
rt_size_t cmux_buffer_write(struct cmux_buffer *buff, rt_uint8_t *input, rt_size_t count)
{
    int c = buff->end_point - buff->write_point;

    count = min(count, cmux_buffer_free(buff));
    if (count > c)
    {
        rt_memcpy(buff->write_point, input, c);
        rt_memcpy(buff->data, input + c, count - c);
        buff->write_point = buff->data + (count - c);
    }
    else
    {
        rt_memcpy(buff->write_point, input, count);
        buff->write_point += count;
        if (buff->write_point == buff->end_point)
            buff->write_point = buff->data;
    }

    return count;
}

/**
 *  parse buffer for searching cmux frame
 *
 * @param buffer        the cmux buffer for cmux object
 *
 * @return  frame       successful
 *          RT_NULL     no frame in the buffer
 */
static struct cmux_frame *cmux_frame_parse(struct cmux_buffer *buffer)
{
    int end;
    int length_needed = 5; /* channel, type, length, fcs, flag */
    rt_uint8_t *data = RT_NULL;
    rt_uint8_t fcs = 0xFF;
    struct cmux_frame *frame = RT_NULL;

    extern rt_uint8_t cmux_crctable[256];

    /* Find start flag */
    while (!buffer->flag_found && cmux_buffer_length(buffer) > 0)
    {
        if (*buffer->read_point == CMUX_HEAD_FLAG)
            buffer->flag_found = 1;
        INC_BUF_POINTER(buffer, buffer->read_point);
    }
    if (!buffer->flag_found) /* no frame started */
        return RT_NULL;

    /* skip empty frames (this causes troubles if we're using DLC 62) */
    while (cmux_buffer_length(buffer) > 0 && (*buffer->read_point == CMUX_HEAD_FLAG))
    {
        INC_BUF_POINTER(buffer, buffer->read_point);
    }

    if (cmux_buffer_length(buffer) >= length_needed)
    {
        data = buffer->read_point;
        frame = (struct cmux_frame *)rt_malloc(sizeof(struct cmux_frame));
        frame->data = RT_NULL;

        frame->channel = ((*data & 0xFC) >> 2);
        fcs = cmux_crctable[fcs ^ *data];
        INC_BUF_POINTER(buffer, data);

        frame->control = *data;
        fcs = cmux_crctable[fcs ^ *data];
        INC_BUF_POINTER(buffer, data);

        frame->data_length = (*data & 254) >> 1;
        fcs = cmux_crctable[fcs ^ *data];
        /* frame data length more than 127 bytes */
        if ((*data & 1) == 0)
        {
            INC_BUF_POINTER(buffer,data);
            frame->data_length += (*data*128);
            fcs = cmux_crctable[fcs^*data];
            length_needed++;
            LOG_D("len_need: %d, frame_data_len: %d.", length_needed, frame->data_length);
        }
        length_needed += frame->data_length;
        if (cmux_buffer_length(buffer) < length_needed)
        {
            cmux_frame_destroy(frame);
            return RT_NULL;
        }
        INC_BUF_POINTER(buffer, data);
        /* extract data */
        if (frame->data_length > 0)
        {
            frame->data = (unsigned char *)rt_malloc(frame->data_length);
            if (frame->data != RT_NULL)
            {
                end = buffer->end_point - data;
                if (frame->data_length > end)
                {
                    rt_memcpy(frame->data, data, end);
                    rt_memcpy(frame->data + end, buffer->data, frame->data_length - end);
                    data = buffer->data + (frame->data_length - end);
                }
                else
                {
                    rt_memcpy(frame->data, data, frame->data_length);
                    data += frame->data_length;
                    if (data == buffer->end_point)
                        data = buffer->data;
                }
                if (CMUX_FRAME_IS(CMUX_FRAME_UI, frame))
                {
                    for (end = 0; end < frame->data_length; end++)
                        fcs = cmux_crctable[fcs ^ (frame->data[end])];
                }
            }
            else
            {
                LOG_E("Out of memory, when allocating space for frame data.");
                frame->data_length = 0;
            }
        }
        /* check FCS */
        if (cmux_crctable[fcs ^ (*data)] != 0xCF)
        {
            LOG_W("Dropping frame: FCS doesn't match. Remain size: %d", cmux_buffer_length(buffer));
            cmux_frame_destroy(frame);
            buffer->flag_found = 0;
            return cmux_frame_parse(buffer);
        }
        else
        {
            /* check end flag */
            INC_BUF_POINTER(buffer, data);
            if (*data != CMUX_HEAD_FLAG)
            {
                LOG_W("Dropping frame: End flag not found. Instead: %d.", *data);
                cmux_frame_destroy(frame);
                buffer->flag_found = 0;
                return cmux_frame_parse(buffer);
            }
            else
            {
            }
            INC_BUF_POINTER(buffer, data);
        }
        buffer->read_point = data;
    }
    return frame;
}

/**
 * save data from serial, push frame into slist and invoke callback function
 *
 * @param   device  the point of device driver structure, ppp_device structure
 * @param   buf     the address of receive data from uart
 * @param   len     the length of receive data
 */
static void cmux_recv_processdata(struct cmux *cmux, rt_uint8_t *buf, rt_size_t len)
{
    rt_size_t count = len;
    struct cmux_frame *frame = RT_NULL;

    cmux_buffer_write(cmux->buffer, buf, count);

    while ((frame = cmux_frame_parse(cmux->buffer)) != RT_NULL)
    {
        /* distribute different data */
        if ((CMUX_FRAME_IS(CMUX_FRAME_UI, frame) || CMUX_FRAME_IS(CMUX_FRAME_UIH, frame)))
        {
            LOG_D("this is UI or UIH frame from channel(%d).", frame->channel);
            if (frame->channel > 0)
            {
                /* receive data from logical channel, distribution them */
                cmux_frame_push(cmux, frame->channel, frame);
                cmux_vcom_isr(cmux, frame->channel, frame->data_length);
            }
            else
            {
                /* control channel command */
                LOG_W("control channel command haven't support.");
                cmux_frame_destroy(frame);
            }
        }
        else
        {
            switch ((frame->control & ~CMUX_CONTROL_PF))
            {
            case CMUX_FRAME_UA:
                LOG_D("This is UA frame for channel(%d).", frame->channel);

                break;
            case CMUX_FRAME_DM:
                LOG_D("This is DM frame for channel(%d).", frame->channel);

                break;
            case CMUX_FRAME_SABM:
                LOG_D("This is SABM frame for channel(%d).", frame->channel);

                break;
            case CMUX_FRAME_DISC:
                LOG_D("This is DISC frame for channel(%d).", frame->channel);

                break;
            }
            cmux_frame_destroy(frame);
        }
    }
}

/**
 *  assemble general data in the format of cmux
 *
 * @param dev           actual serial device
 * @param port          the number of virtual serial
 * @param type          the format of cmux frame
 * @param data          general data
 * @param length        the length of general data
 *
 * @return  length
 */
static rt_size_t cmux_send_data(struct rt_device *dev, int port, rt_uint8_t type, const char *data, int length)
{
    /* flag, EA=1 C port, frame type, data_length 1-2 */
    rt_uint8_t prefix[5] = {CMUX_HEAD_FLAG, CMUX_ADDRESS_EA | CMUX_ADDRESS_CR, 0, 0, 0};
    rt_uint8_t postfix[2] = {0xFF, CMUX_HEAD_FLAG};
    int c, prefix_length = 4;

    /* EA=1, Command, let's add address */
    prefix[1] = prefix[1] | ((CMUX_DHCL_MASK & port) << 2);
    /* cmux control field */
    prefix[2] = type;

    if (length > CMUX_DATA_MASK)
    {
        prefix_length = 5;
        prefix[3] = ((CMUX_DATA_MASK & length) << 1);
        prefix[4] = (CMUX_HIGH_DATA_MASK & length) >> 7;
    }
    else
    {
        prefix[3] = 1 | (length << 1);
    }
    /* CRC checksum */
    postfix[0] = cmux_frame_check(prefix + 1, prefix_length - 1);

    c = rt_device_write(dev, 0, prefix, prefix_length);
    if (c != prefix_length)
    {
        LOG_E("Couldn't write the whole prefix to the serial port for the virtual port %d. Wrote only %d  bytes.", port, c);
        return 0;
    }
    if (length > 0)
    {
        c = rt_device_write(dev, 0, data, length);
        if (length != c)
        {
            LOG_E("Couldn't write all data to the serial port from the virtual port %d. Wrote only %d bytes.", port, c);
            return 0;
        }
    }
    c = rt_device_write(dev, 0, postfix, 2);
    if (c != 2)
    {
        LOG_E("Couldn't write the whole postfix to the serial port for the virtual port %d. Wrote only %d bytes.", port, c);
        return 0;
    }
#ifdef CMUX_DEBUG
    LOG_HEX("CMUX_TX", 32, (const rt_uint8_t *)data, length);
#endif
    return length;
}

/**
 * Receive thread , store serial data
 *
 * @param cmux    the point of cmux object structure
 *
 * @return  RT_EOK  we shouldn't let the receive thread return data, receive thread need keep alive all the time
 */
static int cmux_recv_thread(struct cmux *cmux)
{
    rt_uint32_t event;
    rt_size_t len;
    rt_uint8_t buffer[CMUX_RECV_READ_MAX];

    rt_event_control(cmux->event, RT_IPC_CMD_RESET, RT_NULL);

    while (1)
    {
        rt_event_recv(cmux->event, CMUX_EVENT_RX_NOTIFY, RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR, RT_WAITING_FOREVER, &event);
        if (event & CMUX_EVENT_RX_NOTIFY)
        {
            do
            {
                len = rt_device_read(cmux->dev, 0, buffer, CMUX_RECV_READ_MAX);
                if (len)
                {
                    cmux_recv_processdata(cmux, buffer, len);
                }

            } while (len);
        }
    }

    return RT_EOK;
}

/**
 * cmux object init function
 *
 * @param object        the point of cmux object
 * @param name          the actual serial name
 * @param vcom_num      the channel of virtual channel
 * @param user_data     private data
 *
 * @return  RT_EOK      successful
 *          RT_ENOMEM   allocate memory failed
 */
rt_err_t cmux_init(struct cmux *object, const char *name, rt_uint8_t vcom_num, void *user_data)
{
    static rt_uint8_t count = 1;
    char tmp_name[RT_NAME_MAX] = {0};
    rt_base_t level;

    if (_g_cmux == RT_NULL)
    {
        _g_cmux = object;
    }
    else
    {
        RT_ASSERT(!_g_cmux);
    }

    object->dev = rt_device_find(name);
    if (object->dev == RT_NULL)
    {
        LOG_E("cmux can't find %s.", name);
        return RT_EOK;
    }

    object->vcom_num = vcom_num;
    object->vcoms = rt_malloc(vcom_num * sizeof(struct cmux_vcoms));
    rt_memset(object->vcoms, 0, vcom_num * sizeof(struct cmux_vcoms));
    if (object->vcoms == RT_NULL)
    {
        LOG_E("cmux vcoms malloc failed.");
        return -RT_ENOMEM;
    }

    object->buffer = cmux_buffer_init();
    if (object->buffer == RT_NULL)
    {
        LOG_E("cmux buffer malloc failed.");
        return -RT_ENOMEM;
    }

    rt_snprintf(tmp_name, sizeof(tmp_name), "cmux%d", count);
    object->event = rt_event_create(tmp_name, RT_IPC_FLAG_FIFO);
    if (object->event == RT_NULL)
    {
        LOG_E("cmux event malloc failed.");
        return -RT_ENOMEM;
    }

    object->user_data = user_data;

    level = rt_hw_interrupt_disable();

    rt_slist_init(&object->list);
    rt_slist_append(&cmux_list, &object->list);

    rt_hw_interrupt_enable(level);

    rt_snprintf(tmp_name, sizeof(tmp_name), "cmux%d", count);
    object->recv_tid = rt_thread_create(tmp_name,
                                        (void (*)(void *parameter))cmux_recv_thread,
                                        object,
                                        CMUX_THREAD_STACK_SIZE,
                                        CMUX_THREAD_PRIORITY,
                                        20);
    if (object->recv_tid == RT_NULL)
    {
        LOG_E("cmux receive thread create failed.");
        return -RT_ERROR;
    }

    LOG_I("cmux rely on (%s) init successful.", name);
    return RT_EOK;
}

/**
 * start cmux function
 *
 * @param object    the point of cmux object
 *
 * @return  the result
 */
rt_err_t cmux_start(struct cmux *object)
{
    rt_err_t result = 0;
    struct rt_device *device = RT_NULL;

    /* uart transfer into cmux */
    rt_device_set_rx_indicate(object->dev, cmux_rx_ind);

    if (object->ops->start != RT_NULL)
    {
        result = object->ops->start(object);
        if (result != RT_EOK)
            return result;
    }

    if (object->recv_tid != RT_NULL)
    {
        result = rt_thread_startup(object->recv_tid);
        if (result != RT_EOK)
        {
            LOG_D("cmux receive thread startup failed.");
            return result;
        }
    }

    /* attach cmux control channel into rt-thread device */
    cmux_attach(object, 0, "cmux_ctl", RT_DEVICE_OFLAG_RDWR | RT_DEVICE_FLAG_DMA_RX, RT_NULL);

    device = rt_device_find("cmux_ctl");
    result = rt_device_open(device, RT_DEVICE_OFLAG_RDWR | RT_DEVICE_FLAG_DMA_RX);
    if (result != RT_EOK)
    {
        LOG_E("cmux control channel open failed.");
    }

    return result;
}

/**
 * stop cmux function
 *
 * @param object    the point of cmux object
 *
 * @return  the result
 */
rt_err_t cmux_stop(struct cmux *object)
{
    if (object->ops->stop != RT_NULL)
    {
        object->ops->stop(object);
    }

    /* we should send CMUX_FRAME_DM frame, close cmux control connect channel */
    cmux_send_data(object->dev, 0, CMUX_FRAME_DISC | CMUX_CONTROL_PF, RT_NULL, 0);

    return RT_EOK;
}

/**
 * control cmux function
 *
 * @param object        the point of cmux object
 *
 * @return  RT_ENOSYS   haven't support control function
 */
rt_err_t cmux_control(struct cmux *object, int cmd, void *args)
{
    RT_ASSERT(object != RT_NULL);

    return -RT_ENOSYS;
}

/**
 * initialize virtual channel, and open it
 *
 * @param dev       the point of virtual device
 * @param oflag     the open flag of rt_device
 *
 * @return  the result
 */
static rt_err_t cmux_vcom_open(rt_device_t dev, rt_uint16_t oflag)
{
    rt_err_t result = RT_EOK;
    struct cmux *object = RT_NULL;
    struct cmux_vcoms *vcom = (struct cmux_vcoms *)dev;

    RT_ASSERT(dev != RT_NULL);

    object = _g_cmux;

    /* establish virtual connect channel */
    cmux_send_data(object->dev, (int)vcom->link_port, CMUX_FRAME_SABM | CMUX_CONTROL_PF, RT_NULL, 0);

    return result;
}

/**
 * close virtual channel
 *
 * @param dev       the point of virtual device
 *
 * @return  the result
 */
static rt_err_t cmux_vcom_close(rt_device_t dev)
{
    rt_err_t result = RT_EOK;
    struct cmux *object = RT_NULL;
    struct cmux_vcoms *vcom = (struct cmux_vcoms *)dev;

    object = _g_cmux;

    cmux_send_data(object->dev, (int)vcom->link_port, CMUX_FRAME_DISC | CMUX_CONTROL_PF, RT_NULL, 0);

    return result;
}

/**
 * write data into virtual channel
 *
 * @param dev       the point of virtual device
 * @param pos       offset
 * @param buffer    the data you want to send
 * @param size      the length of buffer
 *
 * @return  the result
 */
static rt_size_t cmux_vcom_write(struct rt_device *dev,
                                 rt_off_t pos,
                                 const void *buffer,
                                 rt_size_t size)
{
    struct cmux *cmux = RT_NULL;
    struct cmux_vcoms *vcom = (struct cmux_vcoms *)dev;
    rt_size_t len;
    cmux = _g_cmux;

    /* use virtual serial, we can write data into actual serial directly. */
    len = cmux_send_data(cmux->dev, (int)vcom->link_port, CMUX_FRAME_UIH, buffer, size);
    return len;
}

/**
 * write data into virtual channel
 *
 * @param dev       the point of virtual device
 * @param pos       offset
 * @param buffer    the buffer you want to store
 * @param size      the length of buffer
 *
 * @return  the result
 */
static rt_size_t cmux_vcom_read(struct rt_device *dev,
                                rt_off_t pos,
                                void *buffer,
                                rt_size_t size)
{
    struct cmux_vcoms *vcom = (struct cmux_vcoms *)dev;

    struct cmux *cmux = RT_NULL;
    rt_bool_t using_status = 0;

    cmux = _g_cmux;
    using_status = vcom->frame_using_status;

    /* The previous frame has been transmitted finish. */
    if (!using_status)
    {
        /* support fifo, we using the first frame */
        vcom->frame = cmux_frame_pop(cmux, (int)vcom->link_port);
        vcom->length = 0;
        vcom->data = RT_NULL;

        /* can't find frame */
        if (vcom->frame == RT_NULL)
        {
            return 0;
        }

        if (size >= vcom->frame->data_length)
        {
            int data_len = vcom->frame->data_length;
            rt_memcpy(buffer, vcom->frame->data, data_len);
            cmux_frame_destroy(vcom->frame);

            return data_len;
        }
        else
        {
            vcom->data = vcom->frame->data;
            vcom->frame_using_status = 1;
            rt_memcpy(buffer, vcom->data, size);
            vcom->data = vcom->data + size;
            vcom->length = vcom->length + size;

            return size;
        }
    }
    else
    {
        /* transmit the rest of frame */
        if (vcom->length + size >= vcom->frame->data_length)
        {
            size_t read_len;
            rt_memcpy(buffer, vcom->data, vcom->frame->data_length - vcom->length);
            vcom->frame_using_status = 0;

            if (vcom->frame->data_length - vcom->length >= 0) {
                read_len = vcom->frame->data_length - vcom->length;
            }
            cmux_frame_destroy(vcom->frame);
            return read_len;
        }
        else
        {
            rt_memcpy(buffer, vcom->data, size);
            vcom->data = vcom->data + size;
            vcom->length = vcom->length + size;

            return size;
        }
    }
}

/* virtual serial ops */
#ifdef RT_USING_DEVICE_OPS
const struct rt_device_ops cmux_device_ops =
{
    RT_NULL,
    cmux_vcom_open,
    cmux_vcom_close,
    cmux_vcom_read,
    cmux_vcom_write,
    RT_NULL,
};
#endif

/**
 * attach cmux into cmux object
 *
 * @param object        the point of cmux object
 * @param link_port     the channel of virtual serial
 * @param alias_name    the name of virtual name
 * @param flag          the type of virtual serial
 * @param user_data     private data
 *
 * @return  RT_EOK          execute successful
 *
 */
rt_err_t cmux_attach(struct cmux *object, int link_port, const char *alias_name, rt_uint16_t flags, void *user_data)
{
    RT_ASSERT(object != RT_NULL);
    struct rt_device *device = RT_NULL;

    if(link_port >= object->vcom_num)
    {
        LOG_E("PORT[%02d] attach failed, please increase CMUX_PORT_NUMBER in the env.", link_port);
        return -RT_EINVAL;
    }

    device = &object->vcoms[link_port].device;
    device->type = RT_Device_Class_Char;
    device->rx_indicate = RT_NULL;
    device->tx_complete = RT_NULL;

#ifdef RT_USING_DEVICE_OPS
    device->ops = &cmux_device_ops;
#else
    device->init = RT_NULL;
    device->open = cmux_vcom_open;
    device->close = cmux_vcom_close;
    device->read = cmux_vcom_read;
    device->write = cmux_vcom_write;
    device->control = RT_NULL;
#endif

    object->vcoms[link_port].link_port = (rt_uint8_t)link_port;

    vcoms_cmux_frame_init(object, link_port);

    /* interrupt mode or dma mode is meaningless, because we don't have buffer for vcom */
    if (flags & RT_DEVICE_FLAG_INT_RX)
        rt_device_register(device, alias_name, RT_DEVICE_FLAG_RDWR | RT_DEVICE_FLAG_STREAM | RT_DEVICE_FLAG_INT_RX);
    else
        rt_device_register(device, alias_name, RT_DEVICE_FLAG_RDWR | RT_DEVICE_FLAG_STREAM | RT_DEVICE_FLAG_DMA_RX);

    return RT_EOK;
}

/**
 * detach cmux from object
 *
 * @param object            the point of cmux object
 * @param alias_name        the name of virtual name
 *
 * @return  RT_EOK          execute successful
 *         -RT_ERROR        error
 */
rt_err_t cmux_detach(struct cmux *object, const char *alias_name)
{
    rt_device_t device = RT_NULL;

    device = rt_device_find(alias_name);
    if (device->open_flag & RT_DEVICE_OFLAG_OPEN)
    {
        LOG_E("You should close vcom (%s) firstly.", device->parent.name);
        return -RT_ERROR;
    }
    cmux_vcom_close(device);

    return RT_EOK;
}
