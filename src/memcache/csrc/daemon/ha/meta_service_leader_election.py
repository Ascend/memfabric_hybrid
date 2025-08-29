# Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.

import os
import time
import threading
from ctypes import cdll
from datetime import datetime, timedelta, UTC
from kubernetes import client, config
from kubernetes.config.config_exception import ConfigException
from kubernetes.client.rest import ApiException


class MmcLogger:
    def __init__(self):
        self.lib_ = cdll.LoadLibrary("libmmc_leader.so")

    def log(self, level: int, msg: str):
        self.lib_.mmc_logger(level, msg.encode('utf-8'))

    def debug(self, msg: str):
        self.log(0, msg)

    def info(self, msg: str):
        self.log(1, msg)

    def warning(self, msg: str):
        self.log(2, msg)

    def error(self, msg: str):
        self.log(3, msg)


logger = MmcLogger()


class MetaServiceLeaderElection:
    """
        提供基本的选主功能，利用以下4个功能，可以实现选主
        1. 更新Lease的renew time，进行尝试选主: update_lease
        2. 检查Leader状态：check_leader_status
        3. 更新Pod为Master：update_pod_to_master
        4. 更新Pod为Backup：update_pod_to_backup
    """

    def __init__(self, lease_name, namespace, pod_name, retry_period=3, log_level=1, log_path="/home/memcache"):
        """
        初始化主备选举器

        :param lease_name: Lease资源名称
        :param namespace: 命名空间
        :param pod_name: 当前Pod名称（用于标识竞选者）
        :param retry_period: 重试间隔（秒）
        """
        # 加载K8s配置
        try:
            config.load_incluster_config()  # 集群内使用
        except ConfigException:
            logger.error(f'Failed in loading config in k8s cluster')
            try:
                config.load_kube_config()  # 本地开发使用
            except ConfigException as e:
                logger.error(f'Failed in loading from ~/.kube/config')

        self.lease_name = lease_name
        self.namespace = namespace
        self.pod_name = pod_name
        self.retry_period = max(retry_period, 1)

        # 初始化API客户端
        self.coordination_v1 = client.CoordinationV1Api()
        self.core_v1 = client.CoreV1Api()

        # 状态标识
        self.is_leader = False
        self.leader_identity = None
        self.lease_duration = 10
        self.stop_event = threading.Event()
        self.renew_thread = None

    def update_lease(self, is_renew=False):
        """刷新Lease"""
        try:
            return self._update_lease(is_renew)
        except ApiException as e:
            if e.status == 409:
                # 多个POD，并发更新，冲突
                for attempt in range(3):
                    time.sleep(1)
                    ret = self._retry_update_lease(is_renew)
                    if ret == 1:
                        # 更新成功，升主
                        return True
                    elif ret == 0:
                        # 有主，且租约未过期，无需更新
                        return False
                    elif ret == 409:
                        # 多个POD，并发更新，冲突
                        continue
                    else:
                        # 其他异常，结束重试
                        break

            logger.error(f'Failed in updating lease {self.pod_name=}, ApiException: {e}')
            return False
        except Exception as e:
            logger.error(f'Failed in updating lease {self.pod_name=}, Exception: {e}')
            return False

    def _retry_update_lease(self, is_renew):
        try:
            return 1 if self._update_lease(is_renew) else 0
        except ApiException as e:
            logger.error(f'Failed in retry updating lease {self.pod_name=}, ApiException: {e}')
            return e.status
        except Exception as e:
            logger.error(f'Failed in updating lease {self.pod_name=}, Exception: {e}')
            return -1

    def _update_lease(self, is_renew):
        # 尝试获取现有Lease
        lease = self.coordination_v1.read_namespaced_lease(
            name=self.lease_name,
            namespace=self.namespace
        )

        # 检查是否需要更新
        renew_time = lease.spec.renew_time
        if renew_time:
            renew_time = datetime.fromisoformat(str(lease.spec.renew_time)).astimezone(UTC)

        holder = lease.spec.holder_identity
        if lease.spec.lease_duration_seconds is not None:
            self.lease_duration = lease.spec.lease_duration_seconds

            # 续约间隔为有效期的 1/3（如 10 秒有效期，每 3 秒续约一次），预留网络延迟缓冲
            if self.retry_period * 3 > self.lease_duration:
                self.retry_period = max(self.lease_duration // 3, 1)

        # 租约未被持有，或持有者是自己，或租约已过期
        current_time = datetime.now(UTC)
        if not holder or holder == self.pod_name or \
                (renew_time and (current_time - renew_time > timedelta(seconds=self.lease_duration))):
            self.__update_lease(is_renew, lease, current_time)
            return True

        logger.debug(
            "Lease=%s is not expired: curHolder=%s, leaseDuration=%s, renewTime=%s, currentTime=%s, "
            "retry_period=%s, my_pod_name=%s",
            self.lease_name, holder, self.lease_duration,
            renew_time, current_time, self.retry_period, self.pod_name)
        return False

    def __update_lease(self, is_renew, lease, current_time):
        # 更新租约
        lease.spec.holder_identity = self.pod_name
        lease.spec.lease_duration_seconds = self.lease_duration
        lease.spec.renew_time = current_time
        if not is_renew:  # 首次获取时更新获取时间
            lease.spec.acquire_time = current_time

        self.coordination_v1.replace_namespaced_lease(
            name=self.lease_name,
            namespace=self.namespace,
            body=lease
        )
        logger.debug(
            "Succeed in updating lease=%s: curHolder=%s, leaseDuration=%s, renewTime=%s, currentTime=%s, "
            "retry_period=%s, my_pod_name=%s",
            self.lease_name, self.pod_name, self.lease_duration,
            current_time, current_time, self.retry_period, self.pod_name)

    def check_leader_status(self):
        """检查当前主节点状态"""
        try:
            lease = self.coordination_v1.read_namespaced_lease(
                name=self.lease_name,
                namespace=self.namespace
            )

            holder = lease.spec.holder_identity
            if not holder:
                return "None"  # 无主节点

            # 检查租约是否过期
            renew_time = lease.spec.renew_time
            if renew_time:
                renew_time = datetime.fromisoformat(str(lease.spec.renew_time)).astimezone(UTC)
            current_time = datetime.now(UTC)
            if renew_time and (current_time - renew_time > timedelta(seconds=self.lease_duration)):
                return "None"  # 租约过期

            return holder
        except ApiException as e:
            logger.error(f'Failed to check leader status {self.pod_name=}, ApiException: {e}')
            return "None"
        except Exception as e:
            logger.error(f'Failed to check leader status {self.pod_name=}, Exception: {e}')
            return "None"

    def update_pod_to_master(self):
        """升主时，调用该方法设置标签为master"""
        # 示例：更新自身标签为backup
        self._update_pod_label({"role": "master"})

    def update_pod_to_backup(self):
        """失去主节点身份时，调用该方法设置标签为backup"""
        # 示例：更新自身标签为backup
        self._update_pod_label({"role": "backup"})

    def _update_pod_label(self, labels):
        """更新当前Pod的标签"""
        try:
            pod = self.core_v1.read_namespaced_pod(
                name=self.pod_name,
                namespace=self.namespace
            )

            if pod.metadata.labels is None:
                pod.metadata.labels = {}
            pod.metadata.labels.update(labels)

            self.core_v1.patch_namespaced_pod(
                name=self.pod_name,
                namespace=self.namespace,
                body=pod
            )
            logger.warning(f'Updated label of {self.pod_name=} to {labels=}')
        except ApiException as e:
            logger.error(f'Failed in updating label of {self.pod_name=} {labels=}, ApiException: {e}')
        except Exception as e:
            logger.error(f'Failed in updating label of {self.pod_name=} {labels=}, Exception: {e}')

    def start_election(self):
        """运行主备选举循环，仅仅用于测试Python功能，核心选举逻辑在C++中"""
        logger.info(f'Start to elect leader, current pod name: {self.pod_name}')

        # 启动续约线程
        self.renew_thread = threading.Thread(target=self._renew_lease, daemon=True)
        self.renew_thread.start()

        try:
            while not self.stop_event.is_set():
                self._check_and_update_leadership()
                time.sleep(self.retry_period)
        finally:
            self.stop_event.set()
            logger.info(f'Stop election {self.pod_name=}')
            if self.renew_thread:
                self.renew_thread.join()

    def _renew_lease(self):
        """定期续约租约"""

        logger.info(f'Start to renew lease, current pod name: {self.pod_name}')
        while not self.stop_event.is_set():
            if self.is_leader:
                success = self.update_lease(is_renew=True)
                if not success:
                    logger.warning(f'Failed in renewing lease and becoming backup, {self.pod_name=}')
                    self.is_leader = False
                    self.update_pod_to_backup()

            time.sleep(self.retry_period)

    def _check_and_update_leadership(self):
        current_leader = self.check_leader_status()
        if current_leader == self.pod_name:
            if not self.is_leader:
                logger.info(f'Pod {self.pod_name} becomes the leader')
                self.is_leader = True
                self.update_pod_to_master()
        else:
            if self.is_leader:
                logger.info(f'Pod {self.pod_name} becomes backup')
                self.is_leader = False
                self.update_pod_to_backup()
            else:
                # 尝试竞选主节点
                if self.update_lease():
                    logger.info(f'Pod {self.pod_name} becomes the leader')
                    self.is_leader = True
                    self.update_pod_to_master()

    def stop_election(self):
        """停止选举循环"""
        self.stop_event.set()
        logger.info(f'Stop in electing leader, {self.pod_name=}')


# 示例用法
if __name__ == "__main__":
    # 从环境变量获取当前Pod信息（在K8s中运行时自动注入）
    POD_NAME = os.getenv("META_POD_NAME", "meta-pod-0")
    NAMESPACE = os.getenv("META_NAMESPACE", "default")
    LEASE_NAME = os.getenv("META_LEASE_NAME", "default")

    election = MetaServiceLeaderElection(
        lease_name=LEASE_NAME,
        namespace=NAMESPACE,
        pod_name=POD_NAME,
        retry_period=5,
        log_level=0,
        log_path="/home/memcache"
    )

    try:
        # 运行选举
        election.start_election()
    except KeyboardInterrupt:
        logger.error("election process aborted")
        election.stop_election()
