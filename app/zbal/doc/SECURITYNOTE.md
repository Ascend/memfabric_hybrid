### 环境变量

- ZBAL使用到的[环境变量](./user_guide/env.md)列表。

### 运行用户建议

- 基于安全性考虑，建议您在执行任何命令时，不建议使用root等管理员类型账户执行，遵循权限最小化原则。

### 文件权限最大值建议

- 建议用户在主机（包括宿主机）及容器中设置运行系统umask值为0027及以上，保障新增文件夹默认最高权限为750，新增文件默认最高权限为640。
- 建议对使用当前项目已有和产生的文件、数据、目录，设置如下建议权限。

| 类型                              | Linux权限参考最大值  |
| --------------------------------- | ------------------  |
| 用户主目录                         | 750（rwxr-x---）    |
| 程序文件(含脚本文件、库文件等)      | 550（r-xr-x---）    |
| 程序文件目录                       | 550（r-xr-x---）    |
| 配置文件                          | 640（rw-r-----）     |
| 配置文件目录                       | 750（rwxr-x---）    |
| 日志文件(记录完毕或者已经归档)      | 440（r--r-----）    |
| 日志文件(正在记录)                 | 640（rw-r-----）    |
| 日志文件目录                       | 750（rwxr-x---）    |
| Debug文件                          | 640（rw-r-----）   |
| Debug文件目录                      | 750（rwxr-x---）    |
| 临时文件目录                       | 750（rwxr-x---）    |
| 维护升级文件目录                    | 770（rwxrwx---）   |
| 业务数据文件                       | 640（rw-r-----）    |
| 业务数据文件目录                    | 750（rwxr-x---）   |
| 密钥组件、私钥、证书、密文文件目录   | 700（rwx—----）     |
| 密钥组件、私钥、证书、加密密文       | 600（rw-------）    |
| 加解密接口、加解密脚本              | 500（r-x------）    |

### 依赖软件声明

当前项目运行依赖软件和安装方式。
1. [CANN](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/900/index/index.html)：安装时请阅读软件注意事项，根据需要选择版本。
2. [Ascend HDK](https://support.huawei.com/enterprise/zh/undefined/ascend-hdk-pid-252764743)：安装时请阅读软件注意事项，根据需要选择版本。
3. [Memfabric_Hybrid](https://pypi.org/project/memfabric-hybrid/)：请选择1.1.0及以后得版本。

### 源码内公网地址

| 类型       | 开源代码地址                                     | 文件名                                                | 公网IP地址/公网URL地址/域名/邮箱地址             | 用途说明                           |
| ---------- | ------------------------------------------------ | ----------------------------------------------------- | ------------------------------------------------ | ---------------------------------- |
| 许可证     | http://license.coscl.org.cn/MulanPSL2            | src/下的文件                                          | http://license.coscl.org.cn/MulanPSL2            | 开源License（木兰PSL v2）协议地址  |
| 第三方代码 | http://www.boost.org/LICENSE_1_0.txt             | third_party/ska/flat_hash_map.h                       | http://www.boost.org/LICENSE_1_0.txt             | flat_hash_map遵循的Boost软件许可证 |
| 依赖三方库 | https://github.com/google/googletest.git         | 文档引用（test/ut/ 测试框架）                         | https://github.com/google/googletest.git         | 单元测试框架依赖                   |
| 项目地址   | https://gitcode.com/Ascend/memfabric_hybrid.git  | README.md                                             | https://gitcode.com/Ascend/memfabric_hybrid.git  | 项目源码仓克隆地址                 |
| 参考文档   | https://github.com/sgl-project/sglang/pull/24575 | doc/user_guide/get_started.md                         | https://github.com/sgl-project/sglang/pull/24575 | SGLang集成补丁参考                 |
| 参考文档   | https://github.com/pytorch/pytorch/issues/161356 | src/csrc/adaptor/pytorch_npu/zbal_pytorch_c10_dma.cpp | https://github.com/pytorch/pytorch/issues/161356 | PyTorch可扩展段模式相关问题参考    |
