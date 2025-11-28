### 通信矩阵

|组件|tcp store|
|----------------|--------|
|源设备|tcp client|
|源IP|device IP|
|源端口|操作系统自动分配，分配范围由操作系统的自身配置决定|
|目的设备|tcp server|
|目的IP|设备地址IP|
|目的端口（侦听）|用户指定，端口号1025~65535|
|协议|TCP|
|端口说明|server与client TCP协议消息接口|
|侦听端口是否可更改|是|
|认证方式|数字证书认证|
|加密方式|TLS 1.3|
|所属平面|业务面|
|版本|所有版本|
|特殊场景|无|

说明：
支持通过接口 `smem_set_conf_store_tls` 配置TLS秘钥证书等，进行tls安全连接，安全选项默认开启，建议用户开启TLS加密配置，以保证通信通信安全，如需关闭加密功能，可以使用下面示例，调用接口关闭。
系统启动后，建议删除本地秘钥证书等信息敏感文件。调用该接口时，传入的文件路径不能包含英文分号、逗号、冒号。
支持通过环境变量 `ACCLINK_CHECK_PERIOD_HOURS`和`ACCLINK_CERT_CHECK_AHEAD_DAYS` 配置证书检查周期与证书过期预警时间

配置TLS调用接口示例：
```c
// 配置关闭tls:
smem_set_conf_store_tls(false, nullptr, 0);

// 配置打开tls:
char *tls_info ="                               \
    tlsCaPath: /etc/ssl/certs/;                 \
    tlsCert: /etc/ssl/certs/server.crt;         \
    tlsCrlPath: /etc/ssl/crl/;                  \
    tlsCrlFile: server_crl1.pem,server_crl2.pem;\
    tlsCaFile: ca.pem1,ca.pem2;                 \
    packagePath: /etc/lib";
int32_t ret = smem_set_conf_store_tls(true, tls_info, strlen(tls_info));

char *tls_pk = "xxx";
char *tls_pk_pw = "xxx";
int32_t ret = smem_set_config_store_tls_key(tls_pk, strlen(tls_pk), tls_pk_pw, strlen(tls_pk_pw), nullptr);
其中，若口令为密文，则需将解密函数作为第五个入参传入smem_set_config_store_tls_key

// 可选，配置每七天检查一次证书:
export ACCLINK_CHECK_PERIOD_HOURS=168
// 可选，配置剩余十四天过期时警告:
export ACCLINK_CERT_CHECK_AHEAD_DAYS=14
```

|字段|含义|Required|
|-|-|-|
| tlsCaPath | ca证书存储路径 | 是 |
| tlsCert | server证书 | 是 |
| tlsCrlPath | 证书吊销列表存储路径 | 否 |
| tlsCrlFile | 证书吊销列表 | 否 |
| tlsCaFile | ca证书列表 | 是 |
| packagePath | OpenSSL lib库路径 | 否 |


| 环境变量 | 说明                                         |
|------|-----------------------------------------------------------|
| ACCLINK_CHECK_PERIOD_HOURS  | 指定证书检查周期（单位：小时），超出范围 [ 24, 24 * 30 ] 或不是整数，则设置默认值7 * 24   |
| ACCLINK_CERT_CHECK_AHEAD_DAYS  | 指定证书预警时间（单位：天），超出范围 [ 7, 180 ] 或不是整数或换算成小时小于检查周期，则设置默认值30 |

### 运行用户建议

- 基于安全性考虑，建议您在执行任何命令时，不建议使用root等管理员类型账户执行，遵循权限最小化原则。

### 文件权限最大值建议

- 建议用户在主机（包括宿主机）及容器中设置运行系统umask值为0027及以上，保障新增文件夹默认最高权限为750，新增文件默认最高权限为640。
- 建议对使用当前项目已有和产生的文件、数据、目录，设置如下建议权限。

| 类型           | Linux权限参考最大值 |
| -------------- | ---------------  |
| 用户主目录                        |   750（rwxr-x---）            |
| 程序文件(含脚本文件、库文件等)       |   550（r-xr-x---）             |
| 程序文件目录                      |   550（r-xr-x---）            |
| 配置文件                          |  640（rw-r-----）             |
| 配置文件目录                      |   750（rwxr-x---）            |
| 日志文件(记录完毕或者已经归档)        |  440（r--r-----）             |
| 日志文件(正在记录)                |    640（rw-r-----）           |
| 日志文件目录                      |   750（rwxr-x---）            |
| Debug文件                         |  640（rw-r-----）         |
| Debug文件目录                     |   750（rwxr-x---）  |
| 临时文件目录                      |   750（rwxr-x---）   |
| 维护升级文件目录                  |   770（rwxrwx---）    |
| 业务数据文件                      |   640（rw-r-----）    |
| 业务数据文件目录                  |   750（rwxr-x---）      |
| 密钥组件、私钥、证书、密文文件目录    |  700（rwx—----）      |
| 密钥组件、私钥、证书、加密密文        | 600（rw-------）      |
| 加解密接口、加解密脚本            |   500（r-x------）        |

### 调用acc_links接口列表

#### 日志模块

| 接口功能描述                | 接口声明                                      |
|-----------------------------|--------------------------------------------|
| 设置自定义日志函数         | `int32_t AccSetExternalLog(void (*func)(int level, const char* msg));` |
| 设置日志打印级别             | `int32_t AccSetLogLevel(int level);`       |

#### TCP服务端模块

| 接口功能描述                | 接口声明                                      |
|-----------------------------|--------------------------------------------|
| 创建TCP服务端           | `static AccTcpServerPtr Create();`         |
| 启动服务端          | `int32_t Start(const AccTcpServerOptions &opt);` |
| TLS认证方式启动服务端           | `int32_t Start(const AccTcpServerOptions &opt, const AccTlsOption &tlsOption);` |
| 停止服务端                  | `void Stop();`                             |
| 连接其余服务端            | `int32_t ConnectToPeerServer(const std::string &peerIp, uint16_t port, const AccConnReq &req, uint32_t maxRetryTimes, AccTcpLinkComplexPtr &newLink);` |
| 注册处理新请求事件函数              | `void RegisterNewRequestHandler(int16_t msgType, const AccNewReqHandler &h);` |
| 注册处理断链事件函数            | `void RegisterLinkBrokenHandler(const AccLinkBrokenHandler &h);` |
| 注册处理新链接事件函数              | `void RegisterNewLinkHandler(const AccNewLinkHandler &h);` |
| 注册密码解密的函数 | `void RegisterDecryptHandler(const AccDecryptHandler &h);` |
| 加载安全认证所需动态库          | `int32_t LoadDynamicLib(const std::string &dynLibPath);` |


### 依赖软件声明

当前项目运行依赖 cann 和 Ascend HDK，安装使用及注意事项参考[CANN](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/81RC1beta1/index/index.html)和[Ascend HDK](https://support.huawei.com/enterprise/zh/undefined/ascend-hdk-pid-252764743)并选择对应版本。

### 源码内公网地址

| 类型   | 开源代码地址      | 文件名      | 公网IP地址/公网URL地址/域名/邮箱地址                     | 用途说明            |
|------  |-----------------|-------------|--------------------------------------------|-------------------|
| 依赖三方库  |  https://github.com/google/googletest.git | .gitmodules | https://github.com/google/googletest.git   | 单元测试框架依赖 |
| 依赖三方库  | https://github.com/sinojelly/mockcpp.git  | .gitmodules | https://github.com/sinojelly/mockcpp.git   | 单元测试框架依赖 |
| license 地址 | 不涉及  | LICENSE | http://www.apache.org/licenses/            | license文件 |
| license 地址 |  不涉及 |  LICENSE | http://www.apache.org/licenses/LICENSE-2.0 | license文件  |
| 代码仓地址 |  https://gitcode.com/Ascend/memfabric_hybrid | 不涉及 | https://gitcode.com/Ascend/memfabric_hybrid | shmem 仓库地址信息 |