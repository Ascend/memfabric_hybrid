# K8S环境中MetaService主备高可用验证流程
本说明用于介绍如何在Kubernetes集群中，验证MetaService的主备高可用流程。在生产环境中，用户可以进行参考配置MetaService的主备高可用。

原理机制：基于K8S的的Service（type=ClusterIP）和Lease资源，实现MetaService的选主功能：
* Service(type=ClusterIp)：对所有的LocalService客户端，提供MetaService服务的统一访问能力，负责将客户端的访问请求路由转发到主MetaService（role=master的Pod为主节点）。
* Lease：提供分布式锁功能，记录主服务Pod名称和租约过期时间。
每个MetaService定时检查Lease资源，判断租约是否过期，如果过期，则尝试更新租约进行选主； 
主服务（leader）则定期续约。 当主节点故障，且租约过期后，其他MetaService节点竞争成为新的主节点（Leader），并更新自身标签role=master。

![metaservice-ha.png](../source/memcache_metaservice_ha.png)

### 1. 环境准备
测试验证推荐用2个节点，本文档基于2节点，要求910b或更新的芯片，2个节点能互相访问。

修改本文档命令中xxxxxxxx为自己的目录或者正确的ip

#### 1.1 确保2台机器时间一致
同时执行如下命令
```shell
sudo timedatectl set-time "2025-10-17 17:30:00"
```

#### 1.2 安装k8s
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
```

```text
k8s_deploy# k describe node | grep Taints
Taints:             <none>
Taints:             <none>
```

查询k8s服务正常
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

NAMESPACE     NAME                                  TYPE        CLUSTER-IP     EXTERNAL-IP   PORT(S)                  AGE     SELECTOR
default       service/kubernetes                    ClusterIP   127.0.0.1      <none>        443/TCP                  29h     <none>
kube-system   service/kube-dns                      ClusterIP   127.0.0.1     <none>        53/UDP,53/TCP,9153/TCP   29h     k8s-app=kube-dns

NAMESPACE     NAME                         DESIRED   CURRENT   READY   UP-TO-DATE   AVAILABLE   NODE SELECTOR            AGE   CONTAINERS    IMAGES                                 SELECTOR
kube-system   daemonset.apps/calico-node   2         2         0       2            0           kubernetes.io/os=linux   29h   calico-node   127.0.0.1:5000/calico/node:v3.28.0   k8s-app=calico-node
kube-system   daemonset.apps/kube-proxy    2         2         2       2            2           kubernetes.io/os=linux   29h   kube-proxy    127.0.0.1:5000/kube-proxy:v1.30.3    k8s-app=kube-proxy

NAMESPACE     NAME                                      READY   UP-TO-DATE   AVAILABLE   AGE   CONTAINERS                IMAGES                                             SELECTOR
kube-system   deployment.apps/calico-kube-controllers   1/1     1            1           29h   calico-kube-controllers   127.0.0.1:5000/calico/kube-controllers:v3.28.0   k8s-app=calico-kube-controllers
kube-system   deployment.apps/coredns                   2/2     2            2           29h   coredns                   127.0.0.1:5000/coredns:v1.11.1                   k8s-app=kube-dns

NAMESPACE     NAME                                                DESIRED   CURRENT   READY   AGE   CONTAINERS                IMAGES                                             SELECTOR
kube-system   replicaset.apps/calico-kube-controllers-d864bb84b   1         1         1       29h   calico-kube-controllers   127.0.0.1:5000/calico/kube-controllers:v3.28.0   k8s-app=calico-kube-controllers,pod-template-hash=d864bb84b
kube-system   replicaset.apps/coredns-88f77f6b5                   2         2         2       29h   coredns                   127.0.0.1:5000/coredns:v1.11.1                   k8s-app=kube-dns,pod-template-hash=88f77f6b5
```

#### 1.3 启动一个镜像仓服务
查看本地是否具有镜像仓服务：
```shell
nerdctl ps -a | grep registry
nerdctl start <实例id>
nerdctl ps
```

如果有类似 registry:2 的容器，说明本地运行了一个镜像仓库服务（默认端口 5000），如果没有，则本地启动一个镜像仓服务（使用nerdctl）
```shell
nerdctl run -d --name registry -p 5000:5000 -v /tmp/registry:/var/lib/registry registry:2
nerdctl start registry
```

查询镜像仓服务正常
```text
k8s_deploy# nerdctl ps -a
CONTAINER ID    IMAGE                           COMMAND                   CREATED        STATUS    PORTS    NAMES
98e7e3607d31    docker.io/library/registry:2    "/entrypoint.sh /etc…"    13 days ago    Up                 registry
```

### 2. 配置文件、日志目录、软件包、镜像准备
在2个节点上，建立下面目录结构

```text
/home/meta/
├── bin
├── config
│   ├── mmc-local.conf
│   └── mmc-meta.conf
├── lib
│   └── libhcom.so
├── logs
│   └── logs
└── memfabric_hybrid-1.0.0_linux_aarch64.run
```

#### 2.1 配置文件
先往config目录下拷贝一份配置文件模板（配置文件可以通过安装run包方式获取）
```shell
bash memfabric_hybrid-1.0.0_linux_aarch64.run
cp -rf /usr/local/memfabric_hybrid/latest/config/* /home/meta/config
```

修改如下配置
1. 开启高可用配置开关

   mmc-meta.conf
   ```ini
   ock.mmc.meta.ha.enable = true
   ```

2. 设置TLS证书相关配置，如不使用TLS可以关闭开关（关闭TLS可能导致安全风险，不建议生产环境采用）

   mmc-meta.conf
   ```ini
   ock.mmc.tls.enable = false
   ock.mmc.config_store.tls.enable = false
   ```

   mmc-local.conf
   ```ini
   ock.mmc.tls.enable = false
   ock.mmc.config_store.tls.enable = false
   ock.mmc.local_service.hcom.tls.enable = false
   ```

3. 设置数据传输协议。这里以host_rdma为例，需要通过show_grids查询rdma网卡ip修改hcom_url中的ip

   mmc-local.conf
   ```ini
   ock.mmc.local_service.hcom_url = tcp://X.X.X.X:7000
   ```

4. 根据启动local-service数量设置world_size
   
   mmc-local.conf
   ```ini
   ock.mmc.local_service.world_size = 2
   ```

5. metaservice落盘日志路径推荐设置到有磁盘挂载的目录, 方便直接查看容器中服务的日志

   mmc-meta.conf
   ```ini
   ock.mmc.log_path = /home/meta/logs
   ```

#### 2.2 依赖库
使用host_rdma数据传输协议时，需要将libhcom.so拷贝至/home/meta/lib下

流水线环境可以在此目录获得最新日构建版本libhcom.so: 

/home/CI_HOME_for_25.2.0/memfabric_hybrid_br_A3_shm_bm_630/ubs_comm-hcom-1.2.0-1.oe2203sp3.aarch64/hcom/lib/

#### 2.3 软件包
下载源码，并参考README内容，编译构建安装包，并放置到/home/meta目录下（pod启动时会自动安装这个目录下的包）

#### 2.4 镜像准备
##### 2.4.1 准备一个已安装好memcache所有运行依赖的镜像
内网环境最新镜像：memcache:1028

##### 2.4.2 将docker镜像导入到nerdctl
```shell
# 查询已有镜像
nerdctl images

# 将docker格式转换为nerdctl格式
docker save my-image:latest -o my-image.tar
nerdctl load -i my-image.tar
```

##### 2.4.3 将准备好的镜像推送至之前创建的镜像仓
```shell
# 推送
nerdctl push <name:tag>
# 查询镜像仓已有镜像列表
curl http://127.0.0.1:5000/v2/_catalog
# 查询镜像仓镜像tag列表
curl http://127.0.0.1:5000/v2/<image-name>/tags/list
```

### 3. 启动高可用服务集群

进行测试前，先删掉之前所有资源
```shell
kubectl delete ns ns-memcache
```
代码仓`/test/k8s_deploy`目录存放了相关的测试验证主备高可用的样例yaml文件，
需要修改meta-pods-demo.yaml和local-pods-demo.yaml文件内容，将 **containers:image** 和 **InitContarners:image** 改为第二步push的镜像。

**注意：`./test`目录下所有的文件，仅作样例参考，存在安全风险，不能直接用于生产环境，
用户可以参考样例进行修改定制**

```shell
# 1.创建命名空间、服务账号、角色，并将服务账号和角色进行绑定授权
kubectl apply -f account-role-demo.yaml

# 2.创建租约Lease
kubectl apply -f meta-lease-lock-demo.yaml

# 3.创建Cluster IP，ClusterIP#targetPort为MetaService的访问端口
kubectl apply -f meta-cluster-ip-demo.yaml

# 4.创建MetaService服务
kubectl apply -f meta-pods-demo.yaml
```

查询集群对外ip，如下例子为10.96.232.72
```shell
k8s_deploy# k get service -n ns-memcache
NAME                          TYPE        CLUSTER-IP    EXTERNAL-IP   PORT(S)               AGE
service-cluster-ip-memcache   ClusterIP   10.96.232.72   <none>        18080/TCP,18090/TCP   24s
```
然后修改 /home/meta/config/mmc-local.conf
```ini
ock.mmc.meta_service_url = tcp://10.96.232.72:18080
ock.mmc.local_service.config_store_url = tcp://10.96.232.72:18090
```

创建Local客户端
```shell
kubectl apply -f local-pods-demo.yaml
```

最后确认全部正常启动
```shell
k8s_deploy# kubectl get pod -n ns-memcache
NAME                  READY   STATUS    RESTARTS   AGE
local-service-pod-0   1/1     Running   0          13s
local-service-pod-1   1/1     Running   0          11s
meta-service-pod-0    1/1     Running   0          164m
meta-service-pod-1    1/1     Running   0          164m
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

#### 4.2 测试memcache功能正常
```shell
# 其中，local-service-pod-0为pod名称，local-service为容器名称
# 进入启动一个local 容器
kubectl exec -it local-service-pod-0 -n ns-memcache -c local-service -- bash

source /usr/local/Ascend/ascend-toolkit/set_env.sh;
source /usr/local/memfabric_hybrid/set_env.sh;
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/home/meta/lib
cd /usr/local/memfabric_hybrid/latest/aarch64-linux/script/k8s_deploy;
# 交互式测试，输入put/get/remove 等命令验证功能正常
python interactive_app.py 
```

#### 4.3 通过以下命令，验证主备HA功能
```shell
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


### 5. FAQ
#### 5.1 内网代理配置参考
[代理配置参考链接](https://3ms.huawei.com/next/groups/index.html#/wiki/detail?groupId=3957217&wikiId=8017808&)

#### 5.2 常见问题排查方法
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
`exec python3 /usr/local/memfabric_hybrid/latest/aarch64-linux/script/ha/test-mmc-meta-ha.py > /home/memcache/logs/mmc-local.log 2>&1`）

### 6. 测试场景

- 2个meta, local1 put，切换meta，get正常 910B device_rdma
- 2个meta, local1 put 2 , 切换meta， local2 get 正常 910B device_rdma
- 2个meta, local1 put 2 ,local1 quit， 切换meta， local2 get 正常 910B device_rdma
- 启动local1，local2；local1 put 1；重启local2；切换meta；local1 get成功 local2 get失败

### 7. 注意事项
1. 建议参考k8s官方文档对k8s集群进行安全加固