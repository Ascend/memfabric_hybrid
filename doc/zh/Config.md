## Memcache Configs

#### MetaService Config

| key                             | value type | default                      | valid range               | description                                                 |
|---------------------------------|------------|------------------------------|---------------------------|-------------------------------------------------------------|
| ock.mmc.meta_service_url        | string     | tcp://127.0.0.1:5000         | tcp://\<ip>\<port>        | port in [1025, 65535]                                       |
| ock.mmc.meta.ha.enable          | bool       | false                        | true/false                | enable meta service master/backup HA in k8s cluster         |
| ock.mmc.log_level               | string     | info                         | debug/info/warn/error     | log level                                                   |
| ock.mmc.log_path                | string     | .                            | relative or absolute path | log path, the absolute path is start with '/'               |
| ock.mmc.log_rotation_file_size  | int        | 20                           | 1 <= n <= 500             | log rotation file size(MB)                                  |
| ock.mmc.log_rotation_file_count | int        | 50                           | 1 <= n <= 50              | log rotation file num                                       |
| ock.mmc.evict_threshold_high    | int        | 70                           | 1 <= n <= 100             | evict threshold, 70 mean 70%                                |
| ock.mmc.evict_threshold_low     | int        | 60                           | 0 <= n <= 99              | after evict threshold                                       |
|                                 |            |                              |                           |                                                             |
| ock.mmc.tls.enable              | bool       | false                        | true/false/1/0            | use TLS to secure communication or not                      |
| ock.mmc.tls.top.path            | string     | /opt/ock/security/           | 0 <= len < 1024           | common top path of the cert files                           |
| ock.mmc.tls.ca.path             | string     | certs/ca.cert.pem            | 0 <= len < 1024           | CA path relative to the top path                            |
| ock.mmc.tls.ca.crl.path         | string     | certs/ca.crl.pem             | 0 <= len < 1024           | CRL path relative to the top path                           |
| ock.mmc.tls.cert.path           | string     | certs/server.cert.pem        | 0 <= len < 1024           | server certificate path relative to the top path            |
| ock.mmc.tls.key.path            | string     | certs/server.private.key.pem | 0 <= len < 1024           | server private key path relative to the top path            |
| ock.mmc.tls.key.pass.path       | string     | certs/server.passphrase      | 0 <= len < 1024           | server private key passphrase path relative to the top path |
| ock.mmc.tls.package.path        | string     | /opt/ock/security/libs/      | 0 <= len < 1024           | openssl dynamic lib absolute path                           |

#### LocalService Config

| key                                    | value type | default                      | valid range           | description                                                 |
|----------------------------------------|------------|------------------------------|-----------------------|-------------------------------------------------------------|
| ock.mmc.meta_service_url               | string     | tcp://127.0.0.1:5000         | tcp://\<ip>\<port>    | port in [1025, 65535], <br/> <ip> is cluster-ip in HA       |
| ock.mmc.log_level                      | string     | info                         | debug/info/warn/error | log level                                                   |
|                                        |            |                              |                       |                                                             |
| ock.mmc.tls.enable                     | bool       | false                        | true/false/1/0        | use TLS to secure communication or not                      |
| ock.mmc.tls.top.path                   | string     | /opt/ock/security/           | 0 <= len < 1024       | common top path of the cert files                           |
| ock.mmc.tls.ca.path                    | string     | certs/ca.cert.pem            | 0 <= len < 1024       | CA path relative to the top path                            |
| ock.mmc.tls.ca.crl.path                | string     | certs/ca.crl.pem             | 0 <= len < 1024       | CRL path relative to the top path                           |
| ock.mmc.tls.cert.path                  | string     | certs/client.cert.pem        | 0 <= len < 1024       | client certificate path relative to the top path            |
| ock.mmc.tls.key.path                   | string     | certs/client.private.key.pem | 0 <= len < 1024       | client private key path relative to the top path            |
| ock.mmc.tls.key.pass.path              | string     | certs/client.passphrase      | 0 <= len < 1024       | client private key passphrase path relative to the top path |
| ock.mmc.tls.package.path               | string     | /opt/ock/security/libs/      | 0 <= len < 1024       | openssl dynamic lib absolute path                           |
| ock.mmc.local_service.world_size       | integer    | 1                            | [1, 1024]             |                                                             |
| ock.mmc.local_service.config_store_url | string     | tcp://127.0.0.1:6000         | tcp://\<ip>\<port>    | port in [1025, 65535]                                       |
| ock.mmc.local_service.hcom_url         | string     | tcp://127.0.0.1:7000         | tcp://\<ip>\<port>    | used in dram pool, port in [1, 65535]                       |
| ock.mmc.local_service.protocol         | string     | sdma                         | sdma/roce             |                                                             |
| ock.mmc.local_service.dram.size        | integer    | 0                            | [0, 1099511627776]    | unit: byte, max: 1TB                                        |
| ock.mmc.local_service.hbm.size         | integer    | 2097152                      | [0, 1099511627776]    | unit: byte, max: 1TB                                        |
|                                        |            |                              |                       |                                                             |
| ock.mmc.client.timeout.seconds         | integer    | 60                           | [1, 600]              |                                                             |


#### Communication Port Matrix(通信矩阵)

| 原设备             | 源IP地址     | 源端口             | 目的设备            | 目的IP地址                 | 目的端口（侦听）                 | 协议            | 端口说明                     | 侦听端口是否可更改 | 认证方式 |
|-----------------|-----------|-----------------|-----------------|------------------------|--------------------------|---------------|:-------------------------|-----------|------|
| Local/Client客户端 | 客户端通信IP地址 | 随机端口（由操作系统自动分配） | meta service    | meta_service_url中的<ip> | meta_service_url中的<port> | TCP           | 用于元数据对象管理                | 是         | TLS  |
| memory fabric实例 | 客户端通信IP地址 | 随机端口（由操作系统自动分配） | memory fabric实例 | config_store_url中的<ip> | config_store_url中的<port> | TCP           | 用于memory fabric中BM信息交换同步 | 是         | TLS  |
| 参与hcom通信的实例     | 客户端通信IP地址 | 随机端口（由操作系统自动分配） | 参与hcom通信的实例     | hcom_url中的<ip>         | hcom_url中的<port>         | TCP/RDMA/SDMA | 用于hcom通信                 | 是         | TLS  |