# CMUX

## 1. 简介

CMUX（Connection Multiplexing ），即连接（串口）多路复用，其功能主要在一个真实的物理通道上虚拟多个通道，每个虚拟通道上的连接和数据通讯可独立进行。

CMUX 软件包常用于蜂窝模块串口复用功能（PPP + AT 模式），以及串口硬件资源受限的设备。

CMUX 软件包特点如下：

- 支持通过真实串口虚拟出多个串口；
- 支持所有基于 GSM0707/ 0710 协议的蜂窝模块；
- 支持无缝接入 PPP 功能；

目前 CMUX 的 GSM 功能支持 Luat Air720, SIM7600, SIM800C 模块，后续会接入更多蜂窝模块。

### 1.1 框架图

![](docs/figures/cmux_frame.png)

- CMUX 是一种类似于传输层的协议，用户使用时无法感知该层；数据传输依赖一个真实串口传输，cmux 层负责解析数据用以分发到不同的 virtual uart ；从而实现一个真实串口虚拟出多个 UART 的目的

- CMUX 在应用场景中多用于 UART, 如有必要也可以支持 SPI 方式

  ### 1.2 目录结构
```shell
cmux
├───docs
│   └───figures                     // 文档使用图片
├───inc                             // 头文件
│   │───gsm
│   │   └─── cmux_chat.h
│   └─── cmux.h
├───sample                          // 示例文件
│   └─── cmux_sample_gsm.c
├───src                             // 源码文件
│   ├───gsm
│   │   ├─── cmux_chat.c
│   │   └─── cmux_gsm.c
│   ├─── cmux_utils.c
│   └─── cmux.c
├───LICENSE                         // 软件包许可证
├───README.md                       // 软件包使用说明
└───SConscript                      // RT-Thread 默认的构建脚本
```

### 1.3 许可证

cmux 软件包遵循 Apache-2.0 许可，详见 LICENSE 文件。

## 2. 获取方式

​	**CMUX 软件包相关配置选项介绍**

```c
  [*] cmux protocol for rt-thread.  --->
      [ ] using cmux debug feature (NEW)
      [*] using for gsm0710 protocol (NEW)
      (5) set cmux max frame list length (NEW)
      (uart2) the real cmux serial prot (NEW)
      (3) the number of cmux modem port (NEW)
      Version (latest)  --->
```

- **using cmux debug feature:** 开启调试日志功能
- **using for gsm0710 protocol:** 打开以支持蜂窝模块
- **set cmux max frame list length:** 设置虚拟端口的 frame 链的最大长度
- **the real cmux serial prot:** cmux 使用的真实串口名称
- **the number of cmux modem port:** 蜂窝模块支持的虚拟串口数量
- **the command for cmux function:** 进入 cmux 模式的命令
- **Version:** 软件包版本号

## 3. 使用方式

cmux 软件包初始化函数如下所示：

**cmux 功能启动函数，该函数自动调用**

```c
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
// 自动初始化
INIT_APP_EXPORT(cmux_sample_start);
#endif
// 命令导出到MSH( cmux_sample_start 变更为cmux_start )
MSH_CMD_EXPORT_ALIAS(cmux_sample_start, cmux_start, a sample of cmux function);
```

* 模块进入 cmux 模式，注册 AT 和 PPP 接口到 CMUX 上；



模块上电后，自动初始化流程如下：

```shell
 \ | /
- RT -     Thread Operating System
 / | \     4.0.2 build Apr 14 2020
 2006 - 2019 Copyright by rt-thread team
lwIP-2.0.2 initialized!
[I/sal.skt] Socket Abstraction Layer initialize success.
[I/ppp.dev] ppp_device(pp) register successfully.
[I/cmux] cmux rely on (uart3) init successful.
[I/cmux.air720] cmux has been control uart3.
msh >[W/chat] <tx: AT, want: OK, retries: 10, timeout: 1> timeout
[I/cmux.sample] cmux sample (uart3) start successful.
[I/cmux.sample] cmux object channel (cmux_at) attach successful.
[I/cmux.sample] cmux object channel (cmux_ppp) attach successful.
```

设备上电初始化完成，模块提示模块已经进入 CMUX 模式，可以使用已经注册的虚拟串口。



你也可以同时使用 PPP_DEVICE 提供 ppp 功能，效果如下：

```shell
 \ | /
- RT -     Thread Operating System
 / | \     4.0.2 build Apr 14 2020
 2006 - 2019 Copyright by rt-thread team
lwIP-2.0.2 initialized!
[I/sal.skt] Socket Abstraction Layer initialize success.
[I/ppp.dev] ppp_device(pp) register successfully.
[I/cmux] cmux rely on (uart3) init successful.
[I/cmux.air720] cmux has been control uart3.
msh >[I/cmux.sample] cmux sample (uart3) start successful.
[I/cmux.sample] cmux object channel (cmux_at) attach successful.
[I/cmux.sample] cmux object channel (cmux_ppp) attach successful.
msh >ppp_start
[I/ppp.dev] ppp_device connect successfully.
ping www.baidu.com
60 bytes from 39.156.66.14 icmp_seq=0 ttl=49 time=126 ms
60 bytes from 39.156.66.14 icmp_seq=1 ttl=49 time=129 ms
60 bytes from 39.156.66.14 icmp_seq=2 ttl=49 time=111 ms
60 bytes from 39.156.66.14 icmp_seq=3 ttl=49 time=111 ms
msh >ready
msh >[D/main] cmux control channel has been open.
[D/main] write data : 9
[D/main] 15 ,Recieve
+CSQ: 29,99

[D/main] 6 ,Recieve
OK
```
在使用 PPP 功能的同时，可以使用虚拟出的 cmux_at 串口同时读取模块信号强度值，具体可以参考[<使用文档>](./docs/cmux_port.md)

## 4. 注意事项

* 使用 PPP 功能详情参考 [PPP_DEVICE](https://github.com/RT-Thread-packages/ppp_device)
* 只有在虚拟串口注册到 rt_device 框架后才能通过 rt_device_find 找到虚拟串口，要注意先后顺序
* 虚拟串口 attach 后并不能直接使用，必须通过 rt_device_open 打开后才能使用，符合 rt_device 的操作流程
* 只有进入 cmux 的命令，没有退出 cmux 的命令；所以说，只能通信模块硬重启，而不能软重启，使用时候要注意

## 5. 联系方式

联系人：xiangxistu

Email: liuxianliang@rt-thread.com



对 CMUX 有疑惑，或者对 CMUX 感兴趣的开发者欢迎入群详细了解。

QQ群：749347156 [<传送门>](https://jq.qq.com/?_wv=1027&k=5KcuPGI)

## 6. 相关文档
CMUX 详细介绍：[<文档>](./docs/cmux_basic.md)

通过 CMUX 使用 PPP_DEVICE 文档：[<使用文档>](./docs/cmux_port.md)

## 7. 致谢

感谢网友 @ya-jeks 在 github 上的 gsmmux 参考代码