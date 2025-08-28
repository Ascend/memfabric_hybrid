### 1. 日志接口
**对应文件：** acc_log.h

**功能：** 设置自定义日志函数以及设置日志打印级别。

1. 设置自定义日志函数
    ```cpp
    int32_t AccSetExternalLog(void (*func)(int level, const char* msg));
    ```

    |参数/返回值|含义|
    |-|-|
    |func|函数指针|
    |level|日志级别，0-debug 1-info 2-warn 3-error|
    |msg|日志内容|
    |返回值|成功返回0，其他为错误码|

1. 设置日志打印级别
    ```cpp
    int32_t AccSetLogLevel(int level);
    ```

    |参数/返回值|含义|
    |-|-|
    |level|日志级别，0-debug 1-info 2-warn 3-error|
    |返回值|成功返回0，其他为错误码|

### 2. 客户端接口
**对应文件：** acc_tcp_client.h

#### 类：AccTcpClient
**功能：** TCP客户端操作。

1. 创建客户端
    ```cpp
    static AccTcpClientPtr Create(const std::string &serverIp, uint16_t serverPort);
    ```

    |参数/返回值|含义|
    |-|-|
    |serverIp|服务端IP|
    |serverPort|服务端端口|
    |返回值|成功返回客户端对象的智能指针，失败返回空指针|

1. 在重试次数内连接服务端
    ```cpp
    int32_t Connect(const AccConnReq &connReq, uint32_t maxConnRetryTimes);
    ```

    |参数/返回值|含义|
    |-|-|
    |connReq|连接服务端的请求头|
    |maxConnRetryTimes|最大重连尝试次数|
    |返回值|成功返回0，失败返回错误码|

1. 以最大重试5次连接服务端
    ```cpp
    int32_t Connect(const AccConnReq &connReq);
    ```

    |参数/返回值|含义|
    |-|-|
    |connReq|连接服务端的请求头|
    |返回值|成功返回0，失败返回错误码|

1. 与服务端断开连接
    ```cpp
    void Disconnect();
    ```

    |参数/返回值|含义|
    |-|-|
    |返回值|无|

1. 设置消息接收超时阈值
    ```cpp
    int32_t SetReceiveTimeout(uint32_t timeoutInUs);
    ```

    |参数/返回值|含义|
    |-|-|
    |timeoutInUs|超时时间, 单位us|
    |返回值|成功返回0，失败返回错误码|

1. 设置消息发送超时阈值
    ```cpp
    int32_t SetSendTimeout(uint32_t timeoutInUs);
    ```

    |参数/返回值|含义|
    |-|-|
    |timeoutInUs|超时时间, 单位us|
    |返回值|成功返回0，失败返回错误码|

1. 发送数据
    ```cpp
    int32_t SendRaw(uint8_t *data, uint32_t len);
    ```

    |参数/返回值|含义|
    |-|-|
    |data|源数据地址，指向有效内存区域至少有len个字节|
    |len|数据长度|
    |返回值|成功返回0，失败返回错误码|

1. 接收数据
    ```cpp
    int32_t ReceiveRaw(uint8_t *data, uint32_t len);
    ```

    |参数/返回值|含义|
    |-|-|
    |data|目标数据地址，指向至少len个字节的内存空间|
    |len|数据长度|
    |返回值|成功返回0，失败返回错误码|

1. 轮询接收数据
    ```cpp
    int32_t PollAndReceiveRaw(uint8_t *data, uint32_t len, int32_t timeoutInUs);
    ```

    |参数/返回值|含义|
    |-|-|
    |data|接收缓冲区地址，指向至少len个字节的内存空间|
    |len|数据长度|
    |timeoutInUs|超时时间, 单位us|
    |返回值|成功返回0，失败返回错误码|

1. 发送数据并附带消息类型
    ```cpp
    int32_t Send(int16_t msgType, uint8_t *data, uint32_t len);
    ```

    |参数/返回值|含义|
    |-|-|
    |msgType|消息类型, 最小值0， 最大值47|
    |data|源数据地址，指向至少len个字节的内存空间|
    |len|数据长度|
    |返回值|成功返回0，失败返回错误码|

1. 接收数据与消息类型
    ```cpp
    int32_t Receive(uint8_t *data, uint32_t len, int16_t &msgType, int16_t &result, uint32_t &acLen);
    ```

    |参数/返回值|含义|
    |-|-|
    |data|接收缓冲区地址，指向至少len个字节的内存空间|
    |len|数据长度|
    |msgType|出参, 消息类型|
    |result|出参, 响应结果|
    |acLen|出参, 响应的数据长度|
    |返回值|成功返回0，失败返回错误码|

1. 轮询接收数据与消息类型
    ```cpp
    int32_t PollAndReceive(uint8_t *data, uint32_t len, int32_t timeoutInUs, int16_t &msgType, int16_t &result, uint32_t &acLen);
    ```

    |参数/返回值|含义|
    |-|-|
    |data|接收缓冲区地址，指向至少len个字节的内存空间|
    |len|数据长度|
    |timeoutInUs|超时时间, 单位us|
    |msgType|出参, 消息类型|
    |result|出参, 接收的响应结果|
    |acLen|出参, 接收的响应的数据长度|
    |返回值|成功返回0，失败返回错误码|

1. 注册处理新请求的事件函数
    ```cpp
    void RegisterNewRequestHandler(int16_t msgType, const AccClientReqHandler &h);
    ```

    |参数/返回值|含义|
    |-|-|
    |msgType|处理函数对应的消息类型, 最小值0， 最大值47|
    |h|处理新请求的函数|
    |返回值|无|

1. 获取服务端IP和端口
    ```cpp
    std::string IpAndPort() const;
    ```

    |参数/返回值|含义|
    |-|-|
    |返回值|服务端IP和端口的字符串|

1. 设置服务端IP和端口
    ```cpp
    void SetServerIpAndPort(std::string serverIp, uint16_t serverPort);
    ```

    |参数/返回值|含义|
    |-|-|
    |serverIp|服务端IP字符串|
    |serverPort|服务端端口|
    |返回值|无|

1. 设置本地IP
    ```cpp
    void SetLocalIp(std::string localIp);
    ```

    |参数/返回值|含义|
    |-|-|
    |localIp|本地IP字符串|
    |返回值|无|

1. 初始化连接
    ```cpp
    int32_t ConnectInit(int &fd);
    ```

    |参数/返回值|含义|
    |-|-|
    |fd|出参, 连接成功后的文件描述符|
    |返回值|成功返回0，失败返回错误码|

1. 设置SSL配置
    ```cpp
    void SetSslOption(const AccTlsOption &tlsOption);
    ```

    |参数/返回值|含义|
    |-|-|
    |tlsOption|tls安全认证相关配置|
    |返回值|无|

1. 设置最大重连次数
    ```cpp
    void SetMaxReconnCnt(uint32_t maxReconnCnt);
    ```

    |参数/返回值|含义|
    |-|-|
    |maxReconnCnt|最大重连次数|
    |返回值|无|

1. 启动轮询线程
    ```cpp
    void StartPolling();
    ```

    |参数/返回值|含义|
    |-|-|
    |返回值|无|

1. 异步执行销毁客户端
    ```cpp
    void Destroy(bool needWait);
    ```

    |参数/返回值|含义|
    |-|-|
    |needWait|是否异步执行|
    |返回值|无|

1. 同步执行销毁客户端
    ```cpp
    void Destroy();
    ```

    |参数/返回值|含义|
    |-|-|
    |返回值|无|

1. 加载安全认证功能所需的动态库
    ```cpp
    int32_t LoadDynamicLib(const std::string &dynLibPath);
    ```

    |参数/返回值|含义|
    |-|-|
    |dynLibPath|动态库目录路径|
    |返回值|成功返回0，失败返回错误码|

### 3. 服务端接口
**对应文件：** acc_tcp_server.h

#### 类：AccTcpServer
**功能：** TCP服务端操作。

1. 创建服务端
    ```cpp
    static AccTcpServerPtr Create();
    ```

    |参数/返回值|含义|
    |-|-|
    |返回值|无|

1. 以开启TLS认证的方式启动服务端
    ```cpp
    int32_t Start(const AccTcpServerOptions &opt);
    ```

    |参数/返回值|含义|
    |-|-|
    |opt|服务端启动配置项|
    |返回值|成功返回0，失败返回错误码|

1. 启动服务端
    ```cpp
    int32_t Start(const AccTcpServerOptions &opt, const AccTlsOption &tlsOption);
    ```

    |参数/返回值|含义|
    |-|-|
    |opt|服务端启动配置项|
    |tlsOption|TLS配置项|
    |返回值|成功返回0，失败返回错误码|

1. 停止服务端
    ```cpp
    void Stop();
    ```

    |参数/返回值|含义|
    |-|-|
    |返回值|无|

1. 以多进程的方式停止服务端
    ```cpp
    void StopAfterFork();
    ```

    |参数/返回值|含义|
    |-|-|
    |返回值|无|

1. 连接其余服务端
    ```cpp
    int32_t ConnectToPeerServer(const std::string &peerIp, uint16_t port, const AccConnReq &req, uint32_t maxRetryTimes, AccTcpLinkComplexPtr &newLink);
    ```

    |参数/返回值|含义|
    |-|-|
    |peerIp|目标服务端IP|
    |port|目标服务端端口|
    |req|连接请求头|
    |maxRetryTimes|最大重试次数|
    |newLink|出参, 连接成功后的链接|
    |返回值|成功返回0，失败返回错误码|

1. 以最大重试30次连接其余服务端
    ```cpp
    int32_t ConnectToPeerServer(const std::string &peerIp, uint16_t port, const AccConnReq &req, AccTcpLinkComplexPtr &newLink);
    ```

    |参数/返回值|含义|
    |-|-|
    |peerIp|目标服务端IP|
    |port|目标服务端端口|
    |req|连接请求头|
    |newLink|出参, 连接成功后的链接|
    |返回值|成功返回0，失败返回错误码|

1. 注册处理新请求的事件函数
    ```cpp
    void RegisterNewRequestHandler(int16_t msgType, const AccNewReqHandler &h);
    ```

    |参数/返回值|含义|
    |-|-|
    |msgType|处理函数对应的消息类型, 最小值0, 最大值47|
    |h|处理新请求的函数|
    |返回值|无|

1. 注册处理请求发送成功的事件函数
    ```cpp
    void RegisterRequestSentHandler(int16_t msgType, const AccReqSentHandler &h);
    ```

    |参数/返回值|含义|
    |-|-|
    |msgType|处理函数对应的消息类型, 最小值0, 最大值47|
    |h|处理请求发送成功的函数|
    |返回值|无|

1. 注册处理断链的事件函数
    ```cpp
    void RegisterLinkBrokenHandler(const AccLinkBrokenHandler &h);
    ```

    |参数/返回值|含义|
    |-|-|
    |h|处理断链的函数|
    |返回值|无|

1. 注册处理新链接的事件函数
    ```cpp
    void RegisterNewLinkHandler(const AccNewLinkHandler &h);
    ```

    |参数/返回值|含义|
    |-|-|
    |h|处理新链接的函数|
    |返回值|无|

1. 加载安全认证功能所需的动态库
    ```cpp
    int32_t LoadDynamicLib(const std::string &dynLibPath);
    ```

    |参数/返回值|含义|
    |-|-|
    |dynLibPath|动态库目录路径|
    |返回值|成功返回0，失败返回错误码|

### 4. Link接口
**对应文件：** acc_tcp_link.h

#### 类：AccTcpLink
**功能：** 客户端侧生成，提供阻塞式的数据操作。

1. 设置上下文
    ```cpp
    void UpCtx(uint64_t context);
    ```

    |参数/返回值|含义|
    |-|-|
    |context|上下文|
    |返回值|无|

1. 获取上下文
    ```cpp
    uint64_t UpCtx() const;
    ```

    |参数/返回值|含义|
    |-|-|
    |返回值|上下文|

1. 获取这个link的简要描述
    ```cpp
    std::string ShortName() const;
    ```

    |参数/返回值|含义|
    |-|-|
    |返回值|简要描述|

1. 获取link远端的IP端口
    ```cpp
    const std::string &GetLinkRemoteIpPort() const;
    ```

    |参数/返回值|含义|
    |-|-|
    |返回值|远端的IP端口|

1. 获取这个link的id
    ```cpp
    uint32_t Id() const;
    ```

    |参数/返回值|含义|
    |-|-|
    |返回值|id|

1. 获取这个link的是否已建立连接
    ```cpp
    bool Established() const;
    ```

    |参数/返回值|含义|
    |-|-|
    |返回值|已建立连接返回true, 否则返回false|

1. 将状态设置为未建立连接状态
    ```cpp
    bool Break();
    ```

    |参数/返回值|含义|
    |-|-|
    |返回值|如果成功设置为未建立连接状态返回true, 其余情况(包括重复调用)返回false|

1. 向对端发送数据
    ```cpp
    int32_t BlockSend(void *data, uint32_t len);
    ```

    |参数/返回值|含义|
    |-|-|
    |data|源数据地址，指向至少len个字节的内存空间|
    |len|数据长度|
    |返回值|成功返回0，失败返回错误码|

1. 向对端发送数据数组
    
    该函数只在启用 *ENABLE_IOV* 宏时可用
    ```cpp
    int32_t BlockSendIOV(struct iovec *iov, int32_t len, int32_t totalDataLen);
    ```

    |参数/返回值|含义|
    |-|-|
    |iov|源数据数组地址|
    |len|数据数组长度,len必须与iov数组的实际长度一致|
    |totalDataLen|总数据长度|
    |返回值|成功返回0，失败返回错误码|

1. 接收由对端发来的数据
    ```cpp
    int32_t BlockRecv(void *data, uint32_t demandLen);
    ```

    |参数/返回值|含义|
    |-|-|
    |data|目标缓冲区地址|
    |demandLen|数据需求长度|
    |返回值|成功返回0，失败返回错误码|

1. 接收由对端发来的数据数组
    
    该函数只在启用 *ENABLE_IOV* 宏时可用
    ```cpp
    int32_t BlockRecvIOV(struct iovec *iov, int32_t len, int32_t totalDataLen);
    ```

    |参数/返回值|含义|
    |-|-|
    |iov|目标缓冲区地址|
    |len|数据数组长度,len必须与iov数组的实际长度一致|
    |totalDataLen|总数据长度|
    |返回值|成功返回0，失败返回错误码|

1. 轮询检查是否有传进的数据
    ```cpp
    int32_t PollingInput(int32_t timeoutInMs) const;
    ```

    |参数/返回值|含义|
    |-|-|
    |timeoutInMs|超时时间阈值, 单位ms|
    |返回值|成功返回0，失败返回错误码|

1. 设置发送消息的超时时间阈值
    ```cpp
    int32_t SetSendTimeout(uint32_t timeoutInUs) const;
    ```

    |参数/返回值|含义|
    |-|-|
    |timeoutInUs|超时时间阈值, 单位us|
    |返回值|成功返回0，失败返回错误码|

1. 设置接收消息的超时时间阈值
    ```cpp
    int32_t SetReceiveTimeout(uint32_t timeoutInUs) const;
    ```

    |参数/返回值|含义|
    |-|-|
    |timeoutInUs|超时时间阈值, 单位us|
    |返回值|成功返回0，失败返回错误码|

1. 启用非阻塞模式
    ```cpp
    int32_t EnableNoBlocking() const;
    ```

    |参数/返回值|含义|
    |-|-|
    |返回值|成功返回0，失败返回错误码|

1. 关闭link
    ```cpp
    void Close();
    ```

    |参数/返回值|含义|
    |-|-|
    |返回值|无|

1. 检查是否已连接
    ```cpp
    bool IsConnected() const;
    ```

    |参数/返回值|含义|
    |-|-|
    |返回值|已连接返回true, 其余情况返回false|


#### 类：AccTcpLinkComplex
**功能：** 服务端侧生成，提供非阻塞式的数据操作。

1. 向对端发送数据
    ```cpp
    int32_t NonBlockSend(int16_t msgType, const AccDataBufferPtr &d, const AccDataBufferPtr &cbCtx);
    ```

    |参数/返回值|含义|
    |-|-|
    |msgType|消息类型, 最小值0, 最大值47|
    |d|源数据对象|
    |cbCtx|回调函数上下文|
    |返回值|成功返回0，失败返回错误码|

1. 把本次发送放入队列中向对端发送数据
    ```cpp
    int32_t NonBlockSend(int16_t msgType, uint32_t seqNo, const AccDataBufferPtr &d, const AccDataBufferPtr &cbCtx);
    ```

    |参数/返回值|含义|
    |-|-|
    |msgType|消息类型, 最小值0, 最大值47|
    |seqNo|本次消息的序列号|
    |d|源数据对象|
    |cbCtx|回调函数上下文|
    |返回值|成功返回0，失败返回错误码|

1. 把本次发送放入队列中向对端发送数据并附带操作码
    ```cpp
    int32_t NonBlockSend(int16_t msgType, int16_t opCode, uint32_t seqNo, const AccDataBufferPtr &d, const AccDataBufferPtr &cbCtx);
    ```

    |参数/返回值|含义|
    |-|-|
    |msgType|消息类型, 最小值0, 最大值47|
    |opCode|操作码|
    |seqNo|本次消息的序列号|
    |d|源数据对象|
    |cbCtx|回调函数上下文|
    |返回值|成功返回0，失败返回错误码|

1. 把本次发送放入队列中向对端发送数据并附带消息头
    ```cpp
    int32_t EnqueueAndModifyEpoll(const AccMsgHeader &h, const AccDataBufferPtr &d, const AccDataBufferPtr &cbCtx) = 0;
    ```

    |参数/返回值|含义|
    |-|-|
    |h|消息头|
    |d|源数据对象|
    |cbCtx|回调函数上下文|
    |返回值|成功返回0，失败返回错误码|

#### 环境变量

| 环境变量                             | 含义                                  |
|----------------------------------|-------------------------------------|
| ACCLINK_CHECK_PERIOD_HOURS    | 证书有效期检测间隔，单位小时                      |
| ACCLINK_CERT_CHECK_AHEAD_DAYS | 距离过期提醒天数                            |