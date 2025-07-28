## Memcache Configs

#### MetaService Config

| key                      | value type | default                      | valid range        | description                                      |
|--------------------------|------------|------------------------------|--------------------|--------------------------------------------------|
| ock.mmc.meta_service_url | string     | tcp://127.0.0.1:5000         | tcp://\<ip>\<port> |                                                  |
|                          |            |                              |                    |                                                  |
| ock.mmc.tls.enable       | bool       | false                        | true/false/1/0     | use TLS to secure communication or not           |
| ock.mmc.tls.top.path     | string     | /opt/ock/security/           | 0 <= len < 1024    | common top path of the cert files                |
| ock.mmc.tls.ca.path      | string     | certs/ca.cert.pem            | 0 <= len < 1024    | CA path relative to the top path                 |
| ock.mmc.tls.ca.crl.path  | string     | certs/ca.crl.pem             | 0 <= len < 1024    | CRL path relative to the top path                |
| ock.mmc.tls.cert.path    | string     | certs/server.cert.pem        | 0 <= len < 1024    | server certificate path relative to the top path |
| ock.mmc.tls.key.path     | string     | certs/server.private.key.pem | 0 <= len < 1024    | server private key path relative to the top path |
| ock.mmc.tls.package.path | string     | /opt/ock/security/libs/      | 0 <= len < 1024    | openssl dynamic lib absolute path                |

#### LocalService Config

| key                                    | value type | default                      | valid range        | description                                      |
|----------------------------------------|------------|------------------------------|--------------------|--------------------------------------------------|
| ock.mmc.meta_service_url               | string     | tcp://127.0.0.1:5000         | tcp://\<ip>\<port> |                                                  |
|                                        |            |                              |                    |                                                  |
| ock.mmc.tls.enable                     | bool       | false                        | true/false/1/0     | use TLS to secure communication or not           |
| ock.mmc.tls.top.path                   | string     | /opt/ock/security/           | 0 <= len < 1024    | common top path of the cert files                |
| ock.mmc.tls.ca.path                    | string     | certs/ca.cert.pem            | 0 <= len < 1024    | CA path relative to the top path                 |
| ock.mmc.tls.ca.crl.path                | string     | certs/ca.crl.pem             | 0 <= len < 1024    | CRL path relative to the top path                |
| ock.mmc.tls.cert.path                  | string     | certs/server.cert.pem        | 0 <= len < 1024    | server certificate path relative to the top path |
| ock.mmc.tls.key.path                   | string     | certs/server.private.key.pem | 0 <= len < 1024    | server private key path relative to the top path |
| ock.mmc.tls.package.path               | string     | /opt/ock/security/libs/      | 0 <= len < 1024    | openssl dynamic lib absolute path                |
|                                        |            |                              |                    |                                                  |
| ock.mmc.local_service.device_id        | integer    | 0                            | [0, 383]           |                                                  |
| ock.mmc.local_service.bm_rank_id       | integer    | 0                            | [0, 1023]          |                                                  |
| ock.mmc.local_service.world_size       | integer    | 1                            | [1, 1024]          |                                                  |
| ock.mmc.local_service.config_store_url | string     | tcp://127.0.0.1:6000         | tcp://\<ip>\<port> |                                                  |
| ock.mmc.local_service.hcom_url         | string     | tcp://127.0.0.1:7000         | tcp://\<ip>\<port> |                                                  |
| ock.mmc.local_service.auto_ranking     | integer    | 1                            | [0, 1]             |                                                  |
| ock.mmc.local_service.bm_id            | integer    | 0                            | [0, 127]           |                                                  |
| ock.mmc.local_service.protocol         | string     | sdma                         | sdma/roce          |                                                  |
| ock.mmc.local_service.dram.size        | integer    | 0                            | [0, 1099511627776] | unit: byte, max: 1TB                             |
| ock.mmc.local_service.hbm.size         | integer    | 2097152                      | [0, 1099511627776] | unit: byte, max: 1TB                             |
|                                        |            |                              |                    |                                                  |
| ock.mmc.client.timeout.seconds         | integer    | 60                           | [1, 600]           |                                                  |
