# 1.启动2个meta服务（可以同节点2个进程，也可以多节点）

开发环境可用（需要把k8s代码本地删掉，不影响功能）
端口改成5001-5002,6001-6002

# 2 安装haproxy

# 3./etc/haproxy/haproxy.cfg 参考配置

```text
#---------------------------------------------------------------------
# Example configuration for a possible web application.  See the
# full configuration options online.
#
#   https://www.haproxy.org/download/1.8/doc/configuration.txt
#
#---------------------------------------------------------------------

global
    log         /var/log/hyproxy local0
    user        root
    group       root
    daemon
    maxconn     4000

defaults
    mode                    tcp          # 关键：使用 TCP 模式（四层代理）
    log                     global
    option                  tcplog
    timeout connect         2s
    timeout client          1h
    timeout server          1h
    timeout check           2s
    retries                 1
    option redispatch

# Service 1: 外部 8001 -> 内部 9001
frontend service1_in
    bind *:5000
    default_backend service1_backend

backend service1_backend
    balance first
    option tcp-check
    default-server inter 2s fall 1 rise 1 on-marked-down shutdown-sessions on-marked-up shutdown-backup-sessions
    # 主节点：使用"first"平衡算法使其优先
    server primary_server_name 127.0.0.1:5001 check weight 256
    # 备节点：标记为backup，并设置较低的权重
    server backup_server_name 127.0.0.1:5002 check backup weight 1

# Service 2: 外部 8002 -> 内部 9002
frontend service2_in
    bind *:6000
    default_backend service2_backend

backend service2_backend
    option tcp-check
    default-server inter 2s fall 1 rise 1 on-marked-down shutdown-sessions on-marked-up shutdown-backup-sessions
    # 主节点：使用"first"平衡算法使其优先
    server primary_server_name 127.0.0.1:6001 check weight 256
    # 备节点：标记为backup，并设置较低的权重
    server backup_server_name 127.0.0.1:6002 check backup weight 1
```

# 效果

主节点2秒内可以通，全部client会连接到主节点
主节点2秒内不通，全部client会连接到备节点