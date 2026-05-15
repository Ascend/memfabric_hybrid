#!/usr/bin/env python
# coding=utf-8
# Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
# ZBAL is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#          http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.
import logging
import os
import time
import random
import torch
import torch.distributed as dist
import torch_npu
from zbal import zbal_init, zbal_uninit, zbal_set_logger_level

logger = logging.getLogger(__name__)
torch_npu.npu.config.allow_internal_format = True
logging.basicConfig(level=logging.INFO)


def test_init_zbal_pg():
    local_rank = int(os.environ["LOCAL_RANK"])
    world_size = int(os.environ["WORLD_SIZE"] or 4)
    gpu_id = local_rank

    zbal_set_logger_level(0)
    local_mem = 256 * 1024 * 1024
    if not zbal_init(world_size, gpu_id, local_rank, local_mem):
        logger.error(f"zbal_init failed on rank {local_rank}.")
        return
    else:
        logger.info(f"zbal_init success on rank {local_rank}\n")

    # init process group
    dist.init_process_group("zbal", rank=local_rank, world_size=world_size)
    global_group = dist.group.WORLD
    backend = global_group._get_backend(torch.device("npu", local_rank))
    global_group_name = backend.get_zbal_comm_name()

    # create a group with same ranks of global group
    global_group2 = dist.new_group(list(range(world_size)), backend="zbal")
    backend2 = global_group2._get_backend(torch.device("npu", local_rank))
    global_group_name2 = backend2.get_zbal_comm_name()

    # create a sub group
    sub_group_rank = [0, 2]
    sub_group_name = ""
    if local_rank in sub_group_rank:
        sub_group = dist.new_group(sub_group_rank, backend="zbal")
        backend = sub_group._get_backend(torch.device("npu", local_rank))
        sub_group_name = backend.get_zbal_comm_name()
        dist.destroy_process_group(sub_group)
        del sub_group
    logger.info(f"init zbal group success on rank {local_rank=} {world_size=} \
        {global_group_name=} {global_group_name2=} {sub_group_name=}")

    sleep_time = random.uniform(0, 4)
    time.sleep(sleep_time)
    dist.barrier()
    logger.info(f"after barrier rank={local_rank} sleep={sleep_time}s finish at time={time.time()}")

    dist.destroy_process_group(global_group)
    del global_group

    if not zbal_uninit():
        logger.error("zbal uninit failed.")


if __name__ == "__main__":
    test_init_zbal_pg()
