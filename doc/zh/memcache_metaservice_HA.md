# K8S环境中MetaService主备高可用验证流程
本说明用于介绍如何在Kubernetes集群中，验证MetaService的主备高可用流程。在生产环境中，用户可以进行参考配置MetaService的主备高可用。

原理机制：基于K8S的的Service（type=ClusterIP）和Lease资源，实现MetaService的选主功能：
* Service(type=ClusterIp)：对所有的LocalService客户端，提供MetaService服务的统一访问能力，负责将客户端的访问请求路由转发到主MetaService（role=master的Pod为主节点）。
* Lease：提供分布式锁功能，记录主服务Pod名称和租约过期时间。
每个MetaService定时检查Lease资源，判断租约是否过期，如果过期，则尝试更新租约进行选主； 
主服务（leader）则定期续约。 当主节点故障，且租约过期后，其他MetaService节点竞争成为新的主节点（Leader），并更新自身标签role=master。

![metaservice-ha.png](../source/memcache_metaservice_ha.png)

### 环境说明

测试验证推荐用2个节点,本文档基于2节点，要求910b或更新的芯片，2个节点能互相访问，部分协议需要有下面的ip，
修改本文档命令中xxxxxxxx为自己的目录或者正确的ip
```text
root@localhost:/home/jxxxxxxxx/memfabrichybrid/test/k8s_deploy# show_gids
DEV     PORT    INDEX   GID                                     IPv4            VER     DEV
---     ----    -----   ---                                     ------------    ---     ---
hns_0   1       0       fe80:0000:0000:0000:b24f:a6ff:fe17:a775                 v1      enp61s0f0
hns_0   1       1       fe80:0000:0000:0000:b24f:a6ff:fe17:a775                 v2      enp61s0f0
hns_1   1       0       fe80:0000:0000:0000:b24f:a6ff:fe17:a777                 v1      enp61s0f2
hns_1   1       1       fe80:0000:0000:0000:b24f:a6ff:fe17:a777                 v2      enp61s0f2
hns_2   1       0       fe80:0000:0000:0000:b24f:a6ff:fe17:a771                 v1      enp189s0f0
hns_2   1       1       fe80:0000:0000:0000:b24f:a6ff:fe17:a771                 v2      enp189s0f0
hns_2   1       2       0000:0000:0000:0000:0000:ffff:3d2f:017a 127.0.0.1     v1      enp189s0f0
hns_2   1       3       0000:0000:0000:0000:0000:ffff:3d2f:017a 127.0.0.1     v2      enp189s0f0
hns_3   1       0       fe80:0000:0000:0000:b24f:a6ff:fe17:a773                 v1      enp189s0f2
hns_3   1       1       fe80:0000:0000:0000:b24f:a6ff:fe17:a773                 v2      enp189s0f2
mlx5_0  1       0       fe80:0000:0000:0000:eaeb:d3ff:fe3a:53cc                 v1      enp67s0f0np0
mlx5_0  1       1       fe80:0000:0000:0000:eaeb:d3ff:fe3a:53cc                 v2      enp67s0f0np0
mlx5_0  1       2       0000:0000:0000:0000:0000:ffff:c0a8:647a 127.0.0.1         v1      enp67s0f0np0
mlx5_0  1       3       0000:0000:0000:0000:0000:ffff:c0a8:647a 127.0.0.1         v2      enp67s0f0np0
mlx5_1  1       0       fe80:0000:0000:0000:eaeb:d3ff:fe3a:53cd                 v1      enp67s0f1np1
mlx5_1  1       1       fe80:0000:0000:0000:eaeb:d3ff:fe3a:53cd                 v2      enp67s0f1np1
mlx5_2  1       0       fe80:0000:0000:0000:eaeb:d3ff:fe3a:4a40                 v1      enp195s0f0np0
mlx5_2  1       1       fe80:0000:0000:0000:eaeb:d3ff:fe3a:4a40                 v2      enp195s0f0np0
mlx5_3  1       0       fe80:0000:0000:0000:eaeb:d3ff:fe3a:4a41                 v1      enp195s0f1np1
mlx5_3  1       1       fe80:0000:0000:0000:eaeb:d3ff:fe3a:4a41                 v2      enp195s0f1np1
```

修改/home/meta/config/mmc-local.conf（路径下面会说明）

```shell
ock.mmc.local_service.hcom_url = tcp://127.0.0.1:7000
```

```shell
# 确保2台机器时间一致，同时执行如下命令
sudo timedatectl set-time "2025-10-17 17:30:00"
```

### 内网python3代理配置

配置代理，解决环境中缺失python3包等问题 (安装环境后，千万去掉代理)
kubectl 命令卡主的情况下，检查是否设置了代理
```shell
export https_proxy=http://127.0.0.1:8080

export http_proxy=http://127.0.0.1:8080
unset http_proxy
unset https_proxy
```

```shell 
# 容器中离线安装kubernetes包（下面准备k8s镜像步骤的时候可能用）
pip download kubernetes==33.1.0 --only-binary=:all: -d .
pip install --no-index --find-links . --force-reinstall kubernetes
# 验证包
python3 -c 'from kubernetes import client, config'
```

[代理配置参考链接](https://3ms.huawei.com/next/groups/index.html#/wiki/detail?groupId=3957217&wikiId=8017808&)

### 安装k8s

[k8s安装包和安装步骤](https://3ms.huawei.com/next/groups/index.html#/wiki/detail?wikiId=8030354&groupId=3957217)

```shell
# 启动k8s命令补全
sudo apt install -y bash-completion
echo "source <(kubectl completion bash)" >> ~/.bashrc
echo "alias k=kubectl" >> ~/.bashrc
echo "complete -F __start_kubectl k" >> ~/.bashrc
source ~/.bashrc
```

替换 master1 为你的控制节点名称（如 k8s-master），让主节点也可以被调度

```shell
kubectl taint node master1 node-role.kubernetes.io/control-plane:NoSchedule-
# 全部为空
k describe node | grep Taints
```

```text
k8s_deploy# k describe node | grep Taints
Taints:             <none>
Taints:             <none>

```

需要准备一个镜像仓，查看本地是否具有镜像仓服务：

```
nerdctl ps -a | grep registry
nerdctl start <实例id>
nerdctl ps
```

如果有类似 registry:2 的容器，说明本地运行了一个镜像仓库服务（默认端口 5000），如果没有，则本地启动一个镜像仓服务（使用
nerdctl）

```
nerdctl run -d --name registry -p 5000:5000 -v /tmp/registry:/var/lib/registry registry:2
nerdctl start registry
```

查询镜像服务正常

```text
k8s_deploy# nerdctl ps -a
CONTAINER ID    IMAGE                             COMMAND                   CREATED         STATUS    PORTS    NAMES
776245e10d4c    127.0.0.1:5000/memcache:0729    "bash"                    24 hours ago    Up                 jchtest
98e7e3607d31    docker.io/library/registry:2      "/entrypoint.sh /etc…"    29 hours ago    Up                 registry
```

查询k8s整个服务正常

```text
k8s_deploy# k get all -A -o wide
NAMESPACE     NAME                                          READY   STATUS    RESTARTS       AGE     IP              NODE      NOMINATED NODE   READINESS GATES
kube-system   pod/calico-kube-controllers-d864bb84b-jsq5n   1/1     Running   0              29h     127.0.0.1   master1   <none>           <none>
kube-system   pod/calico-node-4k7t6                         0/1     Running   29 (27h ago)   29h     127.0.0.1     work1     <none>           <none>
kube-system   pod/calico-node-gqtb6                         0/1     Running   0              29h     127.0.0.1     master1   <none>           <none>
kube-system   pod/coredns-88f77f6b5-g2qhc                   1/1     Running   0              29h     127.0.0.1   master1   <none>           <none>
kube-system   pod/coredns-88f77f6b5-q76xv                   1/1     Running   0              29h     127.0.0.1   master1   <none>           <none>
kube-system   pod/etcd-master1                              1/1     Running   1              29h     127.0.0.1     master1   <none>           <none>
kube-system   pod/kube-apiserver-master1                    1/1     Running   1              29h     127.0.0.1     master1   <none>           <none>
kube-system   pod/kube-controller-manager-master1           1/1     Running   1              29h     127.0.0.1     master1   <none>           <none>
kube-system   pod/kube-proxy-b855z                          1/1     Running   0              29h     127.0.0.1     master1   <none>           <none>
kube-system   pod/kube-proxy-fr29g                          1/1     Running   0              29h     127.0.0.1     work1     <none>           <none>
kube-system   pod/kube-scheduler-master1                    1/1     Running   1              29h     127.0.0.1     master1   <none>           <none>
ns-memcache   pod/local-service-pod-0                       1/1     Running   0              3h49m   127.0.0.1     work1     <none>           <none>
ns-memcache   pod/local-service-pod-1                       1/1     Running   0              3h49m   127.0.0.1     master1   <none>           <none>
ns-memcache   pod/meta-service-pod-0                        1/1     Running   0              3h54m   127.0.0.1     master1   <none>           <none>
ns-memcache   pod/meta-service-pod-1                        1/1     Running   0              3h54m   127.0.0.1     work1     <none>           <none>

NAMESPACE     NAME                                  TYPE        CLUSTER-IP     EXTERNAL-IP   PORT(S)                  AGE     SELECTOR
default       service/kubernetes                    ClusterIP   127.0.0.1      <none>        443/TCP                  29h     <none>
kube-system   service/kube-dns                      ClusterIP   127.0.0.1     <none>        53/UDP,53/TCP,9153/TCP   29h     k8s-app=kube-dns
ns-memcache   service/service-cluster-ip-memcache   ClusterIP   127.0.0.1   <none>        18080/TCP,18090/TCP      3h54m   app=meta-service,role=master

NAMESPACE     NAME                         DESIRED   CURRENT   READY   UP-TO-DATE   AVAILABLE   NODE SELECTOR            AGE   CONTAINERS    IMAGES                                 SELECTOR
kube-system   daemonset.apps/calico-node   2         2         0       2            0           kubernetes.io/os=linux   29h   calico-node   127.0.0.1:5000/calico/node:v3.28.0   k8s-app=calico-node
kube-system   daemonset.apps/kube-proxy    2         2         2       2            2           kubernetes.io/os=linux   29h   kube-proxy    127.0.0.1:5000/kube-proxy:v1.30.3    k8s-app=kube-proxy

NAMESPACE     NAME                                      READY   UP-TO-DATE   AVAILABLE   AGE   CONTAINERS                IMAGES                                             SELECTOR
kube-system   deployment.apps/calico-kube-controllers   1/1     1            1           29h   calico-kube-controllers   127.0.0.1:5000/calico/kube-controllers:v3.28.0   k8s-app=calico-kube-controllers
kube-system   deployment.apps/coredns                   2/2     2            2           29h   coredns                   127.0.0.1:5000/coredns:v1.11.1                   k8s-app=kube-dns

NAMESPACE     NAME                                                DESIRED   CURRENT   READY   AGE   CONTAINERS                IMAGES                                             SELECTOR
kube-system   replicaset.apps/calico-kube-controllers-d864bb84b   1         1         1       29h   calico-kube-controllers   127.0.0.1:5000/calico/kube-controllers:v3.28.0   k8s-app=calico-kube-controllers,pod-template-hash=d864bb84b
kube-system   replicaset.apps/coredns-88f77f6b5                   2         2         2       29h   coredns                   127.0.0.1:5000/coredns:v1.11.1                   k8s-app=kube-dns,pod-template-hash=88f77f6b5

NAMESPACE     NAME                                 READY   AGE     CONTAINERS      IMAGES
ns-memcache   statefulset.apps/local-service-pod   2/2     3h49m   local-service   127.0.0.1:5000/memcache:jchv1
ns-memcache   statefulset.apps/meta-service-pod    2/2     3h54m   meta-service    127.0.0.1:5000/memcache:jchv1
```

### 配置文件，日志文件,安装包准备

在2个节点上，建立下面目录结构

```text
/home/meta/
├── bin
├── config
│   ├── mmc-local.conf
│   └── mmc-meta.conf
├── lib
│   └── libhcom.so
└── logs
    └── logs

```

其中config目录:

```shell
bash mxc-memfabric_hybrid-1.0.0_linux_aarch64.run
cp -rf /usr/local/mxc/memfabric_hybrid/latest/config/* /home/meta/config
```

/home/meta/config/mmc-meta.conf 中日志路径推荐修改如下, 方便直接查看容器中服务的日志

```text
ock.mmc.log_path = /home/meta/logs
```

libhcom.so如果不能用，查询本节点最新的版本

```shell
find /home -type f -name "libhcom.so" -exec ls -lt {} +
root@localhost:/k8s_deploy# find /home -type f -name "libhcom.so" -exec ls -lt {} +
-r-xr-x--- 1 root       root   3601376 Oct 17 17:19 /home/meta/lib/libhcom.so
-rw-r--r-- 1 root       root  14118312 Oct 13 16:42 /home/hsy/libhcom.so
-rw-r--r-- 1 root       root   4200760 Oct 11 14:54 /home/z00497039/lib64/libhcom.so
-rw-r--r-- 1 root       root   4200760 Oct 11 14:53 /home/m30059434/libhcom.so

```

下载源码，并参考README内容，编译构建安装包 output目录
安装包放如下目录（修改成自己的目录），部署pod的时候，会自动安装这个目录下的包

```shell
/home/jxxxxxxxx/mxc-memfabric_hybrid-1.0.0_linux_aarch64.run
# 在主节点安装
source /usr/local/Ascend/ascend-toolkit/set_env.sh
bash output/mxc-memfabric_hybrid-1.0.0_linux_aarch64.run
# /usr/local/mxc/memfabric_hybrid/latest/aarch64-linux/script/k8s_deploy/ 下面是本文档需要的文件
```

### 镜像准备

- nerdctl images查询有可用的镜像，直接使用


- 有nerdctl镜像，但是镜像中环境不满足

```text
# 先建立容器，安装所有依赖包后自测
# 业务服务过程中，由于需要采用python client访问K8S集群资源，因此需要在容器中安装kubernetes python client：`pip3 install kubernetes
nerdctl run -td --shm-size=16g --privileged=true --name jchtest --net=host --entrypoint bash \
-v /var/queue_schedule:/var/queue_schedule \
-v /usr/local/sbin:/usr/local/sbin \
-v /etc/ascend_install.info:/etc/ascend_install.info \
-v /data:/data \
-v /home:/home \
-v /mnt:/mnt \
-v /opt:/opt/ \
-v /usr/local/bin/npu-smi:/usr/local/bin/npu-smi \
-v /usr/local/Ascend/driver:/usr/local/Ascend/driver \
-v /usr/local/Ascend/firmware:/usr/local/Ascend/firmware \
--device=/dev/davinci0:/dev/davinci0 \
--device=/dev/davinci1:/dev/davinci1 \
--device=/dev/davinci2:/dev/davinci2 \
--device=/dev/davinci3:/dev/davinci3 \
--device=/dev/davinci4:/dev/davinci4 \
--device=/dev/davinci5:/dev/davinci5 \
--device=/dev/davinci6:/dev/davinci6 \
--device=/dev/davinci7:/dev/davinci7 \
--device=/dev/davinci_manager:/dev/davinci_manager \
--device=/dev/devmm_svm:/dev/devmm_svm \
--device=/dev/hisi_hdc:/dev/hisi_hdc \
127.0.0.1:5000/memcache:0729

查询k8s容器
crictl ps -a
运行以下命令，将容器提交为镜像
nerdctl commit <容器名> <镜像仓地址:端口>/<镜像名>:<标签>
将新镜像，推送到镜像仓中
nerdctl push <镜像仓地址:端口>/<镜像名>:<标签>
使用nerdctl images查看所有容器镜像
使用nerdctl rmi <镜像仓地址:端口>/<镜像名>:<标签>删除无用镜像
```

- nerdctl images查询不到可用的镜像

```text
把docker镜像上传到k8s
docker save memcache:0729 -o memcache.tar
sudo ctr -n k8s.io images import memcache.tar
nerdctl -n k8s.io tag memcache:0729 127.0.0.1:5000/memcache:0729
nerdctl -n k8s.io push 127.0.0.1:5000/memcache:0729
```

### 3. 采用新的镜像，创建Pod容器

进行测试前，先删掉之前所有资源
```
kubectl delete ns ns-memcache
```
在`./test/k8s_deploy`存放了相关的测试验证主备高可用的样例yaml文件，
需要修改meta-pods-demo.yaml和local-pods-demo.yaml文件内容，将 **containers:image** 和 **InitContarners:image** 改为第二步push的镜像。
修改 /home/meta/config/mmc-meta.conf 路径为使用配置文件的真实路径
/home/jxxxxxxxx修改成自己的目录

**注意：`./test`目录下所有的文件，仅作样例参考，存在安全风险，不能直接用于生产环境，
用户可以参考样例进行修改定制**

```shell
cd /usr/local/mxc/memfabric_hybrid/latest/aarch64-linux/script/k8s_deploy/
#1.创建命名空间、服务账号、角色，并将服务账号和角色进行绑定授权
kubectl apply -f account-role-demo.yaml

# 2.创建租约Lease
kubectl apply -f meta-lease-lock-demo.yaml

#3.创建Cluster IP，ClusterIP#targetPort为MetaService的访问端口
kubectl apply -f meta-cluster-ip-demo.yaml

#4.创建MetaService服务
kubectl apply -f meta-pods-demo.yaml

# 下面命令可以用来查看失败日志
kubectl -n ns-memcache logs meta-service-pod-0 -c meta-service
kubectl -n ns-memcache logs local-service-pod-0 -c local-service
k get all -A -o wide
kubectl get events -n default --sort-by=.metadata.creationTimestamp -A
cat cat /home/meta/logs/logs/mmc-meta.log
# 通过 k get all -A -o wide 查询集群对外ip，如下例子为10.96.232.72
# ns-memcache   service/service-cluster-ip-memcache   ClusterIP   10.96.232.72   <none>        18080/TCP,18090/TCP      3h38m   app=meta-service,role=master
# 然后修改 /home/meta/config/mmc-local.conf
# ock.mmc.meta_service_url = tcp://10.96.232.72:18080
# ock.mmc.local_service.config_store_url = tcp://10.96.232.72:18090
#5.创建Local客户端

kubectl apply -f local-pods-demo.yaml
# 失败后kubectl apply -f local-pods-demo.yaml，修改yaml文件后重试
# 最后确认全部正常启动
kubectl -n ns-memcache logs local-service-pod-0 -c local-service
k get all -A -o wide

root@localhost:/home/jxxxxxxxx/memfabrichybrid/test/k8s_deploy# k get all -A -o wide
NAMESPACE     NAME                                          READY   STATUS    RESTARTS       AGE     IP              NODE      NOMINATED NODE   READINESS GATES
ns-memcache   pod/local-service-pod-0                       1/1     Running   0              4h27m   127.0.0.1     work1     <none>           <none>
ns-memcache   pod/local-service-pod-1                       1/1     Running   0              4h27m   127.0.0.1     master1   <none>           <none>
ns-memcache   pod/meta-service-pod-0                        1/1     Running   0              4h31m   127.0.0.1     master1   <none>           <none>
ns-memcache   pod/meta-service-pod-1                        1/1     Running   0              4h31m   127.0.0.1     work1     <none>           <none>
```

注意:
* mmc-meta.conf中meta_service_url为\<PodIP:targetPort\>
* mmc-local.conf中meta_service_url为\<ClusterIP:Port\>
* 为了方便测试，在样例yaml文件中，均采用了host网络启动Pod；在生产环境中需要修改，但是要保证客户端的PodIP可以互相访问。
* 所有K8S资源均需要置于同一命名空间中

### 4. 查询服务运行状态，并验证选主流程

#### 4.1 通过以下命令，查询服务运行状况

创建成功后查看集群中的Pods
```
# 查询所有的Pods
kubectl get pods -A -o wide 

# 查询指定命名空间的Pods
kubectl get pods -n ns-memcache -o wide 
```

拉起Pod后，查看meta-pods的详情，通过标签`role=master`观察当前的主备节点
```
查询meta-service-pod-0的详情
kubectl describe pod meta-service-pod-0 -n ns-memcache

查询meta-service-pod-1的详情
kubectl describe pod meta-service-pod-1 -n ns-memcache
```

查询ClusterIP详情，Endpoints为当前主服务的PodIP和端口号
```
kubectl describe service service-cluster-ip-memcache -n ns-memcache
```

查询租约Lease详情（Holder为当前主节点。此外，可以通过修改LeaseDurationSeconds，实时修改租约的过期时间）
```
kubectl describe lease lease-memcache -n ns-memcache
```

#### 测试功能正常

```shell
# 其中，local-service-pod-0为pod名称，local-service为容器名称
# 进入启动一个local 容器
kubectl exec -it local-service-pod-0 -n ns-memcache -c local-service -- bash
export MMC_LOCAL_CONFIG_PATH=/home/meta/config/mmc-local.conf;
source /usr/local/Ascend/ascend-toolkit/set_env.sh;
source /usr/local/mxc/memfabric_hybrid/set_env.sh;
cat /usr/local/Ascend/driver/version.info;
cd /usr/local/mxc/memfabric_hybrid/latest/aarch64-linux/script/k8s_deploy;
# 交互式测试，输入put，get，remove 等命令验证功能正常
python interactive_app.py 
```
#### 4.2 通过以下命令，验证主备HA功能

通过下面命令进入local容器

```shell
kubectl exec -it meta-service-pod-0 -n ns-memcache -c meta-service -- bash
export MMC_LOCAL_CONFIG_PATH=/home/meta/config/mmc-local.conf;
source /usr/local/Ascend/ascend-toolkit/set_env.sh;
source /usr/local/mxc/memfabric_hybrid/set_env.sh;
cat /usr/local/Ascend/driver/version.info;
cd /usr/local/mxc/memfabric_hybrid/latest/aarch64-linux/script/k8s_deploy/
# 交互式测试，输入put，get，remove 等命令验证功能正常
python interactive_app.py 

# 获取主节点pod
kubectl get leases -n ns-memcache 
# 主节点执行，查询日志
cat /home/meta/logs/logs/mmc-meta.log
# 杀掉主节点 观察kubectl get leases -n ns-memcache  是否变化，功能是否正常
kubectl delete pod meta-service-pod-0 -n ns-memcache
```

基于K8S机制，删除一个Pod后，K8S会自动拉起一个新的Pod，以维持Pod数量的稳定。
所以当我们删除主节点Pod后，K8S会重新拉起该Pod；如果在租约有效期内，主节点无法恢复，则从节点会竞争成为新的主节点。

假设当前主节点为meta-service-pod-0，删除主Pod 0 
`kubectl delete pod meta-service-pod-0 -n ns-memcache`

通过`kubectl describe pod meta-service-pod-1 -n ns-memcache`, 查询Pod 1是否成为Leader主节点 ，
同时注意观察ClusterIP和Lease详情，是否一同变化


### 5. FAQ，常见问题排查方法

直接查看local-pod-0日志
`kubectl logs local-service-pod-0 -n ns-memcache `

```shell
 kubectl get events -n default --sort-by=.metadata.creationTimestamp -A
 kubectl -n ns-memcache logs local-service-pod-0 -c change-pod-ip --previous
```
如何进入Pod容器内部
```
kubectl exec -it local-service-pod-0 -n ns-memcache -c local-service -- bash
kubectl exec -it local-service-pod-1 -n ns-memcache -c local-service -- bash
kubectl exec -it meta-service-pod-0 -n ns-memcache -c meta-service -- bash
其中，local-service-pod-0为pod名称，local-service为容器名称
```

**建议**：<br>启动容器服务时，将服务日志写入宿主机或共享目录中，便于观察业务运行状况，或定位问题。需要将宿主机目录挂在到Pod容器中。
<br>在样例yaml文件中，已经将home目录挂载到了容器内部，因此可以：
<br>将meta日志路径配置为`/home/memcache`，则对应的业务日志会写到`/home/memcache/logs/mmc-meta.log`文件中
<br>将local的测试输出重定向到`/home/memcache/logs/mmc-local.log`文件中<br>
（注意local-pods-demo.yaml中的业务启动命令
`exec python3 /usr/local/mxc/memfabric_hybrid/latest/aarch64-linux/script/ha/test-mmc-meta-ha.py > /home/memcache/logs/mmc-local.log 2>&1`）

### 测试场景

- 2个meta, local1 put，切换meta，get正常 910B device_rdma
- 2个meta, local1 put 2 , 切换meta， local2 get 正常 910B device_rdma
- 2个meta, local1 put 2 ,local1 quit， 切换meta， local2 get 正常 910B device_rdma
- 启动local1，local2；local1 put 1；重启local2；切换meta；local1 get成功 local2 get失败

```text

2025-10-18 17:15:04.152892 ERROR [HCOM service_channel_imp.cpp:1749] Prepare callback failed 121
2025-10-18 17:15:04.152920 ERROR [HCOM service_channel_imp.cpp:1827] callback or multi error, cbRet:121,ret:0
2025-10-18 17:15:04.152938 ERROR [HCOM service_channel_imp.cpp:1996] Failed to read 121
2025-10-18 17:15:04.152948 error 635 pid[635] [HYBM hybm_data_op_host_rdma.cpp:184] Failed to copy host data to remote host memory ret: 121
2025-10-18 17:15:04.152964 error 635 pid[2025-10-18 17:15:04.152965635 WARN [HCOM net_rdma_driver_oob.cpp:1555] Handle Ep state broken, Ep id 9012724841836445715 , try call Ep broken handle]
[HYBM hybm_data_op_host_rdma.cpp:573] copy GVA to local host failed: 121
2025-10-18 17:15:04.152989 error 635 pid[635] [HYBM hybm_entity_default.cpp:693] Host RDMA data copy direction: 6, failed : 121
2025-10-18 17:15:04.153012 error 635 [MMC mmc_client_default.cpp:302] client mmc_client get key_pid333 read data failed.
2025-10-18 17:15:04.153021 error 635 [MMC mmc_client.cpp:70] mmc_client get key key_pid333 failed!, error code 121
2025-10-18 17:15:04.153026 error 635 [MMC mmcache_store.cpp:825] Failed to get key key_pid333, error code: 121

```