# go 版本

go version go1.24.12 linux/arm64

# 安装和测试

```shell
yum install etcd.aarch64
nohup etcd \
  --name test-etcd \
  --data-dir /tmp/etcd-data \
  --listen-client-urls http://0.0.0.0:12335 \
  --advertise-client-urls http://127.0.0.1:12335 \
  --listen-peer-urls http://0.0.0.0:12336 \
  --initial-advertise-peer-urls http://127.0.0.1:12336 \
  --initial-cluster test-etcd=http://127.0.0.1:12336 \
  --initial-cluster-token tkn1 \
  --initial-cluster-state new &

  etcdctl --endpoints=http://127.0.0.1:12335 put k v

  etcdctl --endpoints=http://127.0.0.1:12335 get k

  # List all keys and their values
etcdctl --endpoints=http://127.0.0.1:12335 get "" --prefix

# Delete all keys from the store
etcdctl --endpoints=http://127.0.0.1:12335 del "" --prefix
  
```

# 编译

```shell
go mod init etcd-client-cgo
go mod tidy
go build -o libetcd_client_v3.so -buildmode=c-shared etcd_client_v3.go
```

# copy libetcd_client_v3.so 到LD_LIBRARY_PATH

# 常见问题