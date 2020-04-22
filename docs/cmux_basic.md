## 1. CMUX 的实现原理

GSM 07.10 协议支持

<img src="./figures/cmux_course.png" alt="cmux_course" style="zoom:90%;" />

​    以上是一次 CMUX 建立，通话和销毁的基本流程，因为牵扯到真实串口到虚拟串口的交互，所以可以在一个物理串口上同时实现多个功能。在 PPP 拨号启用时，仍然可以调用 AT 命令，也可以使用 Modem 功能同时支持数据通话。

## 2. CMUX 协议

​	多路复用协议提供在单个物理通信通道之上虚拟出多个并行的逻辑通信通道的能力，一般应用于TE(Terminal Equipment)与MS(Mobile Station)之间，TE相当于智能手机的AP端，MS相当于智能手机的MODEM端。多路复用协议的实现效果如图：

<img src="./figures/protocol_stack.png" alt=" " style="zoom:75%;" />

>    实际使用中，TE 端的 MUX 向 MS 端的 MUX 发起通道建立请求，设置通道参数等，是主动的一方；
>
>   MS端的MUX等待TE端的服务请求，根据自身能力提供相应服务。

1. **启动CMUX服务** ：向模块发送 AT + CMUX 命令
2. **建立DLC服务**  ：建立数据连接
3. **数据服务**
4. 功耗控制
5. **释放DLC服务**
6. **关闭服务**
7. 控制服务

#### 2.1 CMUX 数据格式

| Flag        | Address | Control |   Length   | Information | FCS    | Flag        |
| ----------- | ------- | ------- | :--------: | ----------- | ------ | ----------- |
| 0xF9(basic) | 地址域  | 控制域  | 数据域长度 | 实际数据域  | 校验域 | 0xF9(basic) |

**地址域：**

| Bit No. | 1    | 2    | 3    | 4    | 5    | 6    | 7    | 8    |
| ------- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- |
| Signal  | EA   | C/R  | DLCI | DLCI | DLCI | DLCI | DLCI | DLCI |

**控制域：**

| Frame  Type                                   | 1    | 2    | 3    | 4    | 5    | 6    | 7    | 8    | 备注 |
| --------------------------------------------- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- |
| SABM (Set Asynchronous Balanced Mode)         | 1    | 1    | 1    | 1    | P/F  | 1    | 0    | 0    |      |
| UA (Unnumbered Acknowledgement)               | 1    | 1    | 0    | 0    | P/F  | 1    | 1    | 0    |      |
| DM (Disconnected Mode)                        | 1    | 1    | 1    | 1    | P/F  | 0    | 0    | 0    |      |
| DISC (Disconnect)                             | 1    | 1    | 0    | 0    | P/F  | 0    | 1    | 0    |      |
| UIH(Unnumbered Information with Header check) | 1    | 1    | 1    | 1    | P/F  | 1    | 1    | 1    |      |
| UI (Unnumbered Information)                   | 1    | 1    | 0    | 0    | P/F  | 0    | 0    | 0    | 可选 |

1. SABM SABM命令帧，异步平衡模式
2. UA  UA回应帧，对SABM和DISC这两个命令帧的确认
3. DM  如果在链路断开状态，收到DISC命令，就应该发一个DM作为响应
4. DISC DISC命令用于终止通道
5. UIH  UIH命令帧/响应帧，相对于 UI 帧只对地址域，控制域和长度域校验
6. UI  UI命令帧/响应帧

#### 2.2 基础介绍

1. CMUX 不同帧类型介绍

   ```shell
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
   ```
