
# -*- coding: utf-8 -*-
# Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.

import os
import re
import math
import torch

from TensorRTL.tensor_rtl.core.ptensor import PTensor, PTensorSet
from TensorRTL.tensor_rtl.core.executor import BatchP2PExecutor, MemoryFabricExecutor

from TensorRTL.examples.verl.utils import get_prefix_name
from TensorRTL.examples.verl.config_interface import Engine
from TensorRTL.examples.verl.model_adaptor.qwen_adaptor import QwenAdaptor
from TensorRTL.examples.verl.model_adaptor.qwen_adaptor import QwenAdaptor as Qwen3Adaptor
from TensorRTL.examples.verl.model_adaptor.qwen_adaptor import QwenMoeAdaptor as Qwen3MoeAdaptor
from TensorRTL.examples.verl.utils import (
    generate_transfer_device_mesh,
    generate_transfer_ep_device_mesh,
    qkv_from_megatron_to_sglang
)
from TensorRTL.tensor_rtl.core.backend_config import MfBmConfig
from TensorRTL.tensor_rtl.utils.utils import get_dtype_size
from memfabric_hybrid import bm

adapter_dict = {
    "Qwen2ForCausalLM": QwenAdaptor,
    "Qwen3ForCausalLM": Qwen3Adaptor,
    'Qwen3MoeForCausalLM': Qwen3MoeAdaptor
}


class VerlSglangReshardPreprocessor:
    def __init__(
        self,
        actor_module=None,
        infer_model=None,
        train_model=None,
        world_size: int = 8,
        train_config=None,
        infer_config=None,
        train_parallel_state=None,
        infer_parallel_state=None,
        param_name_mapping=None,
        model_adaptor_name=None,
        device_mesh=None,
        pipeline_overlap=False,
        backend='hccl'
    ) -> None:
        self.pipeline_overlap = pipeline_overlap
        self.backend = backend
        self.train_config = train_config
        self.device_mesh = device_mesh
        model_adaptor = adapter_dict.get(model_adaptor_name, None)
        if model_adaptor is None:
            raise RuntimeError(f"[TensorRTL] {model_adaptor_name} is not adapted!")

        self.train_parallel_state = train_parallel_state
        self.train_model = train_model
        self.model_adaptor = model_adaptor(train_config)
        train_tp = self.train_config.tensor_model_parallel_size
        infer_tp = self.device_mesh["infer_tp"].mesh.size()[0]
        train_param_config = self.model_adaptor.build_param_config(
            self.train_model, train_tp, infer_tp, Engine.MEGATRON
        ) or {}
        self.model_adaptor.param_config = train_param_config

        self.layer_list_meta = self.get_global_model_info(
            self.model_adaptor.param_name_mapping, 
            train_parallel_state, 
            device_mesh
            )

        self.infer_model = self.get_infer_model()
        if infer_model is not None:
            self.infer_model = infer_model
        infer_param_config = self.model_adaptor.build_infer_param_config(
            self.infer_model, train_tp, infer_tp, train_param_config
        ) or {}

        self.self_rank = torch.distributed.get_rank()
        self.world_size = torch.distributed.get_world_size()
        self.pool_num = 2
        if self.backend == "hccl":
            self.executor = BatchP2PExecutor()
        else:
            bm_config = MfBmConfig(mem_type=bm.BmMemType.DEVICE, data_op_type=bm.BmDataOpType.SDMA,
                                   local_dram_size=0, local_hbm_size=os.environ.get("MF_HBM_SIZE", 1024 * 1024 * 512),
                                   rank_world_size=self.world_size * self.pool_num)
            self.executor = MemoryFabricExecutor(bm_config, executor_type=MemoryFabricExecutor.ExecutorType.WRITER)
        self.model_adaptor.param_config = train_param_config | infer_param_config

        self.available_pool = [i * self.world_size for i in range(self.pool_num)]
        self.cached_bucket_op_dict = {}
        self.max_bucket_id = 0
        self.cur_bucket_id = 0
        self.cur_bucket_op = []
        self.expert_weight = {}
        self.expert_map_dict = {}

    def get_layer_list_meta(self):
        return self.layer_list_meta

    def get_infer_model(self):
        from torch._subclasses.fake_tensor import FakeTensorMode
        module = torch.nn.Module()
        fake_mode = FakeTensorMode()

        def add_dot_param(module: torch.nn.Module, name: str, tensor):
            if '.' not in name: 
                module.register_parameter(name, torch.nn.Parameter(tensor))
                return

            path, param_name = name.rsplit('.', 1)
            *comps, last = path.split('.')
            cur = module
            for comp in comps:
                if not hasattr(cur, comp):
                    setattr(cur, comp, torch.nn.Module())
                cur = getattr(cur, comp)
            if not hasattr(cur, last):
                setattr(cur, last, torch.nn.Module())
            leaf = getattr(cur, last)
            leaf.register_parameter(param_name, torch.nn.Parameter(tensor))

        with fake_mode:
            for weight_info in self.layer_list_meta:
                name = weight_info[2]
                size = weight_info[3]
                param = torch.empty(*size, device=torch.npu.current_device())
                add_dot_param(module, name, param)
        return module

    def get_global_model_info(self, param_name_mapping, train_parallel_state, device_mesh):
        train_tp = self.train_config.tensor_model_parallel_size
        num_layers = self.train_config.num_layers
        train_pp = self.train_config.pipeline_model_parallel_size
        train_pp_rank = train_parallel_state.get_pipeline_model_parallel_rank()
        meta_info = []

        def rewrite(name: str) -> str:
            megatron_patterns = param_name_mapping
            infer_name = name.replace("decoder", "model")
            for old, new in megatron_patterns.items():
                infer_name = infer_name.replace(f"{old}", f"{new}")
            return infer_name

        def generate_param_size(partition_config):
            if partition_config.shard_dim == -1:
                global_param_size = param_size
            else:
                if param.ndim == 1:
                    global_param_size = torch.Size([int(param_size[0] * train_tp)])
                elif param.ndim == 2:
                    global_param_size_2d = int(param_size[partition_config.shard_dim] * train_tp)
                    if partition_config.shard_dim == 1:
                        global_param_size = torch.Size((param_size[0], global_param_size_2d))
                    else:
                        global_param_size = torch.Size((global_param_size_2d, param_size[1]))
            return global_param_size

        for name, param in self.train_model.state_dict().items():
            if param is None:
                continue
            param_size = param.size()
            partition_config = self.model_adaptor.param_config[get_prefix_name(name)]
            global_param_size = param_size
            global_param_size = generate_param_size(partition_config)
            layer_per_stage = num_layers / train_pp

            if re.search(r'layers\.(\d+)\.', name) is not None:
                old_layer_num = int(re.search(r'layers\.(\d+)\.', name).group(1))
                new_layer_num = int(layer_per_stage * train_pp_rank + old_layer_num)
                global_name = re.sub(r'layers\.(\d+)\.', "layers." + str(new_layer_num) + ".", name)
                global_name = rewrite(global_name)
            else:
                global_name = rewrite(name)
            meta_info.append(
                (train_pp_rank, 
                name, 
                global_name, 
                global_param_size, 
                param.dtype, 
                partition_config.shard_dim, 
                param.ndim,
                partition_config.tp_partition, 
                partition_config.ep_partition, 
                partition_config.fused, 
                partition_config.fused_size
                ))
        obj_spec_output = [None] * train_pp
        torch.distributed.all_gather_object(
            object_list=obj_spec_output, obj=meta_info, group=train_parallel_state.get_pipeline_model_parallel_group()
        )
        layer_list_meta = [
            item 
            for sublist in obj_spec_output 
            for item in sublist
            ]
        return layer_list_meta
    
    def per_tensor_generator(self):
        t_model = self.train_model.state_dict()
        infer_tp = self.device_mesh["infer_tp"].mesh.size()[0]

        def combine_sharded_expert_weight(global_name, expert_weight):
            current_device_mesh, update_device_mesh = generate_transfer_ep_device_mesh(
                self.train_config.num_moe_experts, 
                self.train_config.expert_model_parallel_size, 
                self.train_config.expert_tensor_parallel_size,
                self.train_config.pipeline_model_parallel_size,
                infer_tp,
                1,
                1,
                train_pp_rank
                )
            expert_param = expert_weight[(key_name, param_dtype, shard_dim, ndim)] 
            global_name = re.sub(r'\d+$', '', global_name)
            ptensor_lists = []
            for old in current_device_mesh:
                tensors = expert_param.pop(0) if self.self_rank in old else [None]
                ptensor_lists.append(PTensor(tensor=tensors,
                                    dtype=param_dtype,
                                    ndim=ndim,
                                    device_mesh=old,
                                    global_size=global_param_size[0],
                                    shard_dim=shard_dim,
                                    rank=self.self_rank,
                                    backend='hccl',
                        )
                    )
            ptensorset = PTensorSet(ptensor_lists)
            ptensorset.transfer_map(update_device_mesh)
            op_list = ptensorset.get_transfer_list()
            self.executor.execute(op_list)
            self.executor.wait()
            _tensor = ptensorset.collect_tensor()
            tensor = torch.stack(_tensor, dim=0)
            return global_name, tensor

        expert_weight = {}
        for weight_info in self.layer_list_meta:
            train_pp_rank, name, global_name, global_param_size, param_dtype, shard_dim, ndim, \
                    tp_partition, ep_partition, fused, fused_size = weight_info
            current_device_mesh, update_device_mesh = generate_transfer_device_mesh(
                self.train_config.tensor_model_parallel_size,
                self.train_config.pipeline_model_parallel_size,
                infer_tp,
                1,
                tp_partition,
                train_pp_rank)
            
            if 'mlp.experts.w13_weight' in global_name or 'mlp.experts.w2_weight' in global_name:
                tp_partition = False
                if 'mlp.experts.w13_weight' in global_name:
                    global_param_size, train_params = generate_global_param_size(
                        t_model, name, fused, global_param_size, shard_dim, fused_size
                    )
                    key_name = 'mlp.experts.w13_weight'
                    expert_weight.setdefault((key_name, param_dtype, shard_dim, ndim), [])
                    expert_weight[(key_name, param_dtype, shard_dim, ndim)].append(train_params[0])

                elif 'mlp.experts.w2_weight' in global_name:
                    key_name = 'mlp.experts.w2_weight' 
                    global_param_size, train_params = generate_global_param_size(
                        t_model, name, fused, global_param_size, shard_dim, fused_size
                    )
                    expert_weight.setdefault((key_name, param_dtype, shard_dim, ndim), [])
                    expert_weight[(key_name, param_dtype, shard_dim, ndim)].append(train_params[0])

                if len(expert_weight[(key_name, param_dtype, shard_dim, ndim)]) == \
                    (self.train_config.num_moe_experts // self.train_config.expert_model_parallel_size):
                    global_name, tensor = combine_sharded_expert_weight(global_name, expert_weight)
                    expert_weight[(key_name, param_dtype, shard_dim, ndim)] = []

                else:
                    continue
            else:
                global_param_size, train_params = generate_global_param_size(
                    t_model, name, fused, global_param_size, shard_dim, fused_size
                )
                global_name, tensor = self.ptensor_reshard(
                    train_params, name, 
                    global_name, 
                    param_dtype, 
                    shard_dim, ndim, 
                    global_param_size, 
                    current_device_mesh, 
                    update_device_mesh
                    )
            yield global_name, tensor
   
    def get_sglang_reshard_info(self):
        return self.per_tensor_generator if self.backend == "hccl" else self.per_tensor_generator_mf

    def get_sglang_reshard_replay(self):
        return self.per_tensor_generator_mf_replay

    def ptensor_reshard(self, 
        train_params, 
        name, 
        global_name, 
        param_dtype, 
        shard_dim, 
        ndim, 
        global_param_size, 
        current_device_mesh, 
        update_device_mesh
    ):
        tensor_list = []

        for idx, train_param in enumerate(train_params):
            ptensor_list = []            
            for device_mesh in current_device_mesh:
                if self.self_rank in device_mesh and train_param is None:
                    raise RuntimeError(f'rank {self.self_rank} current rank do not have specific parameters {name}')
                ptensor_list.append(PTensor(tensor=train_param,
                    dtype=param_dtype,
                    ndim=ndim,
                    device_mesh=device_mesh,
                    global_size=global_param_size[idx],
                    shard_dim=shard_dim,
                    rank=self.self_rank,
                    backend='hccl',
                ))
            ptensorset = PTensorSet(ptensor_list)
            ptensorset.transfer_map(update_device_mesh)
            op_list = ptensorset.get_transfer_list()

            self.executor.execute(op_list)
            self.executor.wait()
            _tensor = ptensorset.collect_tensor()

            tensor_list.append(_tensor)

        if len(tensor_list) > 1:
            for fused_tensor in zip(*tensor_list):
                tensor = torch.cat(fused_tensor, shard_dim)
        else:
            tensor = tensor_list[0][0] # id0 表示fused0， id1表示当前device的ptensor的tensor(非Moe每个device只有一个ptensor)

        if 'self_attn.qkv_proj' in global_name:
            if not (self.train_config.num_query_groups >= self.train_config.tensor_model_parallel_size and \
            self.train_config.num_query_groups >= self.device_mesh["infer_tp"].mesh.size()[0]):
                raise RuntimeError(f"[TensorRTL] heads should be divided in tp ranks.")

            tensor = qkv_from_megatron_to_sglang(
                qkv_proj=tensor,
                num_query_groups=self.train_config.num_query_groups,
                num_attention_heads=self.train_config.num_attention_heads,
                q_head_dim=self.train_config.kv_channels,
                k_head_dim=self.train_config.kv_channels,
                v_head_dim=self.train_config.kv_channels,
            )

        return global_name, tensor

    def per_tensor_generator_mf_replay(self):
        num_buckets = self.max_bucket_id + 1
        params_dict = self.train_model.state_dict()

        for bucket_id in range(num_buckets):
            op_list = self.cached_bucket_op_dict.get(bucket_id, [])
            for op in op_list:
                op.buffer = get_tensor_from_tensor_slice(
                    params_dict[op.buffer_name], op.shard_dim, op.tensor_slice
                )
            self.executor.execute(op_list)
            if bucket_id != num_buckets - 1:
                yield
        self._clear_ptensor_op_memory()
        return

    def per_tensor_generator_mf(self):
        t_model = self.train_model.state_dict()
        infer_tp_size = self.device_mesh["infer_tp"].mesh.size()[0]

        param_offset_dict = self._init_param_offset_dict(infer_tp_size)

        for weight_info in self.layer_list_meta:
            (train_pp_rank, name, global_name, global_param_size, param_dtype, shard_dim, ndim,
             tp_partition, ep_partition, fused, fused_size) = weight_info

            if 'mlp.experts.w13_weight' in global_name or 'mlp.experts.w2_weight' in global_name:
                expert_global_name = re.sub(r'\d+$', '', global_name)
                key_name = 'mlp.experts.w13_weight' if 'mlp.experts.w13_weight' in global_name \
                           else 'mlp.experts.w2_weight'
                global_param_size, train_params = generate_global_param_size(
                    t_model, name, fused, global_param_size, shard_dim, fused_size
                )
                self.expert_weight.setdefault((key_name, param_dtype, shard_dim, ndim), [])
                self.expert_weight[(key_name, param_dtype, shard_dim, ndim)].append(train_params[0])
                self.expert_map_dict.setdefault(train_params[0].data_ptr(), name)

                if len(self.expert_weight[(key_name, param_dtype, shard_dim, ndim)]) < \
                      (self.train_config.num_moe_experts // self.train_config.expert_model_parallel_size):
                    continue

                current_device_mesh, update_device_mesh = self._generate_transfer_device_mesh(
                    self.train_config, infer_tp_size, train_pp_rank, tp_partition, is_ep=True
                )
                total_op_list, simulate_bucket_id = self._process_expert_weights(
                    weight_info, param_offset_dict, expert_global_name, infer_tp_size,
                    key_name, current_device_mesh, update_device_mesh, global_param_size
                )

                yield from self._execute_expert_weights(
                    total_op_list, simulate_bucket_id, expert_global_name, param_offset_dict, infer_tp_size
                )
                self.expert_weight[(key_name, param_dtype, shard_dim, ndim)] = []
            else:
                global_param_size, train_params = generate_global_param_size(
                    t_model, name, fused, global_param_size, shard_dim, fused_size
                )
                current_device_mesh, update_device_mesh = self._generate_transfer_device_mesh(
                    self.train_config, infer_tp_size, train_pp_rank, tp_partition, is_ep=False
                )
                yield from self._process_regular_weights(
                    weight_info, param_offset_dict, train_params, infer_tp_size,
                    current_device_mesh, update_device_mesh, global_param_size
                )

            if len(self.cur_bucket_op) > 0:
                self.executor.execute(self.cur_bucket_op)
                self.cur_bucket_op = []

    def _clear_ptensor_op_memory(self):
        for _, op_list in self.cached_bucket_op_dict.items():
            for op in op_list:
                op.buffer = None

    def _init_param_offset_dict(self, infer_tp_size):
        param_offset_dict = {}
        max_bucket_id = 0
        cur_offset = 0
        bucket_id = 0
        expert_offset_init = []

        for weight_info in self.layer_list_meta:
            (train_pp_rank, name, global_name, global_param_size, param_dtype, shard_dim, ndim,
            tp_partition, ep_partition, fused, fused_size) = weight_info

            is_expert_weight = 'mlp.experts.w13_weight' in global_name or 'mlp.experts.w2_weight' in global_name

            if is_expert_weight:
                bucket_id, cur_offset, param_offset_dict, max_bucket_id = self._handle_expert_offset_dict(
                    global_name, expert_offset_init, bucket_id, cur_offset,
                    weight_info, param_offset_dict, max_bucket_id, infer_tp_size
                )
            else:
                bucket_id, cur_offset, param_offset_dict, max_bucket_id = self._handle_regular_offset_dict(
                    global_name, bucket_id, cur_offset, weight_info, infer_tp_size, param_offset_dict, max_bucket_id
                )

        self.max_bucket_id = max_bucket_id
        return param_offset_dict

    def _handle_expert_offset_dict(
        self, global_name, expert_offset_init, bucket_id, cur_offset,
        weight_info, param_offset_dict, max_bucket_id, infer_tp_size
    ):
        expert_global_name = re.sub(r'\d+$', '', global_name)
        if expert_global_name in expert_offset_init:
            return bucket_id, cur_offset, param_offset_dict, max_bucket_id

        expert_offset_init.append(expert_global_name)
        local_expert_num = self.train_config.num_moe_experts // infer_tp_size
        single_param_size = get_dtype_size(weight_info[4]) * math.prod(weight_info[3])

        for i in range(local_expert_num):
            single_expert_name = expert_global_name + str(i)
            if cur_offset + single_param_size > self.executor.bm_config.local_hbm_size:
                bucket_id += 1
                cur_offset = 0

            param_offset_dict[single_expert_name] = (bucket_id, cur_offset)
            cur_offset += single_param_size

        max_bucket_id = max(max_bucket_id, bucket_id)
        return bucket_id, cur_offset, param_offset_dict, max_bucket_id

    def _handle_regular_offset_dict(
        self, global_name, bucket_id, cur_offset, weight_info, infer_tp_size, param_offset_dict, max_bucket_id
    ):
        i_tp_size = infer_tp_size if weight_info[7] else 1
        param_size = get_dtype_size(weight_info[4]) * math.prod(weight_info[3]) // i_tp_size

        if cur_offset + param_size > self.executor.bm_config.local_hbm_size:
            bucket_id += 1
            cur_offset = 0

        param_offset_dict[global_name] = (bucket_id, cur_offset)
        cur_offset += param_size

        max_bucket_id = max(max_bucket_id, bucket_id)
        return bucket_id, cur_offset, param_offset_dict, max_bucket_id

    def _generate_transfer_device_mesh(self, train_config, infer_tp_size, train_pp_rank, tp_partition, is_ep=False):
        if is_ep:
            current_device_mesh, update_device_mesh = generate_transfer_ep_device_mesh(
                train_config.num_moe_experts,
                train_config.expert_model_parallel_size,
                train_config.expert_tensor_parallel_size,
                train_config.pipeline_model_parallel_size,
                infer_tp_size,
                1,
                1,
                train_pp_rank
            )
        else:
            current_device_mesh, update_device_mesh = generate_transfer_device_mesh(
                train_config.tensor_model_parallel_size,
                train_config.pipeline_model_parallel_size,
                infer_tp_size,
                1,
                tp_partition,
                train_pp_rank
            )

        for idx, device_mesh in enumerate(update_device_mesh):
            update_device_mesh[idx] = [rank + self.world_size for rank in device_mesh]

        return current_device_mesh, update_device_mesh

    def _calculate_expert_offsets(self, global_name, expert_global_name, infer_tp_size):
        local_expert_offset = int(global_name[len(expert_global_name):])
        infer_expert_offset = 0

        if self.train_config.expert_model_parallel_size > infer_tp_size:
            ep_ratio = self.train_config.expert_model_parallel_size // infer_tp_size
            ep_rank_offset = self.train_parallel_state.get_expert_model_parallel_rank() % ep_ratio
            local_expert_num = self.train_config.num_moe_experts // self.train_config.expert_model_parallel_size
            infer_expert_offset = ep_rank_offset * local_expert_num
        else:
            infer_expert_num = self.train_config.num_moe_experts // infer_tp_size
            local_expert_offset = local_expert_offset % infer_expert_num

        return infer_expert_offset

    def _process_expert_weights(
        self, weight_info, param_offset_dict, expert_global_name, infer_tp_size,
        key_name, current_device_mesh, update_device_mesh, global_param_size
    ):
        (train_pp_rank, name, global_name, _, param_dtype, shard_dim, ndim,
        tp_partition, ep_partition, fused, fused_size) = weight_info

        infer_expert_offset = self._calculate_expert_offsets(global_name, expert_global_name, infer_tp_size)

        expert_param = self.expert_weight[(key_name, param_dtype, shard_dim, ndim)]
        ptensor_lists = []

        for old in current_device_mesh:
            tensors = expert_param.pop(0) if torch.distributed.get_rank() in old else [None]
            ptensor_lists.append(PTensor(
                tensor=tensors,
                dtype=param_dtype,
                ndim=ndim,
                device_mesh=old,
                global_size=global_param_size[0],
                shard_dim=shard_dim,
                rank=torch.distributed.get_rank(),
                backend='mf',
                check_sanity=False)
            )
        ptensorset = PTensorSet(ptensor_lists)
        ptensorset.transfer_map(update_device_mesh)
        origin_op_list = ptensorset.get_transfer_list()

        current_op_list = []
        expert_offset_dict = {}
        total_op_list = []
        simulate_bucket_id = self.cur_bucket_id

        for op in origin_op_list:
            if not op.is_send:
                continue

            expert_offset_dict.setdefault(op.dst_rank, 0)
            infer_expert_idx = expert_offset_dict[op.dst_rank] + infer_expert_offset

            infer_expert_name = expert_global_name + str(infer_expert_idx)
            if param_offset_dict[infer_expert_name][0] != simulate_bucket_id:
                if len(current_op_list) > 0:
                    total_op_list.append((current_op_list, simulate_bucket_id))
                simulate_bucket_id = param_offset_dict[infer_expert_name][0]
                current_op_list = []

            op.gva_ptr += param_offset_dict[infer_expert_name][1]
            expert_offset_dict[op.dst_rank] += 1

            if self.pipeline_overlap:
                pool_offset = self.available_pool[simulate_bucket_id % len(self.available_pool)]
                op.pool_rank = op.dst_rank - pool_offset

            current_op_list.append(op)
            op.shard_dim = -1
            buffer_name = self.expert_map_dict[op.buffer.data_ptr()]
            op.buffer_name = buffer_name
            self.cached_bucket_op_dict.setdefault(simulate_bucket_id, [])
            self.cached_bucket_op_dict[simulate_bucket_id].append(op)

        if len(current_op_list) > 0:
            total_op_list.append((current_op_list, simulate_bucket_id))

        return total_op_list, simulate_bucket_id

    def _process_regular_weights(
        self, weight_info, param_offset_dict, train_params,
        infer_tp_size, current_device_mesh, update_device_mesh, global_param_size
    ):
        (train_pp_rank, name, global_name, _, param_dtype, shard_dim, ndim,
        tp_partition, ep_partition, fused, fused_size) = weight_info

        if param_offset_dict[global_name][0] != self.cur_bucket_id:
            self.executor.execute(self.cur_bucket_op)
            self.cur_bucket_op = []
            yield
            self.cur_bucket_id = param_offset_dict[global_name][0]

        fused_offset_list = init_fused_offset(
            train_params, global_param_size, param_dtype, infer_tp_size, tp_partition
        )
        for idx, train_param in enumerate(train_params):
            ptensor_list = []
            for device_mesh in current_device_mesh:
                ptensor_list.append(PTensor(
                    tensor=train_param,
                    dtype=param_dtype,
                    ndim=ndim,
                    device_mesh=device_mesh,
                    global_size=global_param_size[idx],
                    shard_dim=shard_dim,
                    rank=torch.distributed.get_rank(),
                    backend='mf',
                    check_sanity=False)
                )
            ptensorset = PTensorSet(ptensor_list)
            ptensorset.transfer_map(update_device_mesh)
            op_list = ptensorset.get_transfer_list()
            self._update_regular_operations(
                op_list, fused_offset_list, idx, param_offset_dict, global_name, shard_dim, name
            )
            self.cur_bucket_op.extend(op_list)

    def _update_regular_operations(
        self, op_list, fused_offset_list, idx, param_offset_dict, global_name, shard_dim, name
    ):
        for op in op_list:
            op.gva_ptr += (fused_offset_list[idx] + param_offset_dict[global_name][1])
            if self.pipeline_overlap and op.is_send:
                pool_offset = self.available_pool[self.cur_bucket_id % len(self.available_pool)]
                op.pool_rank = op.dst_rank - pool_offset
            op.shard_dim = shard_dim
            op.buffer_name = name
            self.cached_bucket_op_dict.setdefault(self.cur_bucket_id, [])
            self.cached_bucket_op_dict[self.cur_bucket_id].append(op)

    def _execute_expert_weights(
        self, total_op_list, simulate_bucket_id, expert_global_name, param_offset_dict, infer_tp_size
    ):
        if len(total_op_list) > 0:
            yield from self._execute_long_total_op_list(
                total_op_list, simulate_bucket_id, expert_global_name, param_offset_dict, infer_tp_size
            )
        else:
            yield from self._execute_total_op_list(expert_global_name, param_offset_dict, infer_tp_size)

    def _execute_long_total_op_list(
        self, total_op_list, simulate_bucket_id, expert_global_name, param_offset_dict, infer_tp_size
    ):
        while len(total_op_list) > 0:
            op_list, simulate_bucket_id = total_op_list.pop(0)
            if simulate_bucket_id != self.cur_bucket_id:
                self.executor.execute(self.cur_bucket_op)
                self.cur_bucket_op = []
                yield
                self.cur_bucket_id = simulate_bucket_id

            self.cur_bucket_op.extend(op_list)

            if len(total_op_list) > 0:
                self.executor.execute(self.cur_bucket_op)
                self.cur_bucket_op = []
                yield
                self.cur_bucket_id += 1
            else:
                infer_expert_name = expert_global_name + str(self.train_config.num_moe_experts // infer_tp_size - 1)
                if param_offset_dict[infer_expert_name][0] > self.cur_bucket_id:
                    self.executor.execute(self.cur_bucket_op)
                    self.cur_bucket_op = []
                    yield
                    self.cur_bucket_id = param_offset_dict[infer_expert_name][0]

    def _execute_total_op_list(self, expert_global_name, param_offset_dict, infer_tp_size):
        infer_expert_name = expert_global_name + str(self.train_config.num_moe_experts // infer_tp_size - 1)
        if param_offset_dict[infer_expert_name][0] > self.cur_bucket_id:
            self.executor.execute(self.cur_bucket_op)
            self.cur_bucket_op = []
            yield
            self.cur_bucket_id = param_offset_dict[infer_expert_name][0]


def get_tensor_from_tensor_slice(buffer, shard_dim, tensor_slice):
    if shard_dim == -1:
        _register_buffer = buffer
    elif buffer.ndim == 1:
        _register_buffer = buffer[tensor_slice[0]:tensor_slice[1]]
    else:
        if shard_dim == 0:
            _register_buffer = buffer[tensor_slice[0]: tensor_slice[1], :]
        elif shard_dim == 1:
            _register_buffer = buffer[:, tensor_slice[0]: tensor_slice[1]].contiguous()
    return _register_buffer


def generate_global_param_size(t_model, name, fused, global_param_size, shard_dim, fused_size=None):
    def split_fuse_size(train_params, fused_size, shard_dim):
        offset = 0
        train_params_ = []
        for size in fused_size:
            if shard_dim == 0:
                train_params_.append(train_params[0][offset: offset + size, :])
            elif shard_dim == 1:
                train_params_.append(train_params[0][:, offset: offset + size])
            offset += size
        return train_params_

    train_params = [t_model[name]] if name in t_model.keys() else [None]

    def scale_func(param_size, shard_dim, r):
        return tuple(int(x * r) if i == shard_dim else x for i, x in enumerate(param_size))

    if fused:
        if train_params == [None]:
            train_params = [None for _ in fused_size]
        else:
            train_params = split_fuse_size(train_params, fused_size, shard_dim)
        fused_ratio = [chunk_size / sum(fused_size) for chunk_size in fused_size]
        global_param_size = [scale_func(global_param_size, shard_dim, r) for r in fused_ratio]
    else:
        global_param_size = [global_param_size]

    return global_param_size, train_params


def init_fused_offset(train_params, global_param_size, param_dtype, infer_tp_size, tp_partition):
    fused_offset_list = []
    fused_offset = 0
    for idx, _ in enumerate(train_params):
        i_tp_size = infer_tp_size if tp_partition else 1
        fused_tensor_size = get_dtype_size(param_dtype) * math.prod(global_param_size[idx]) // i_tp_size
        fused_offset_list.append(fused_offset)
        fused_offset += fused_tensor_size

    return fused_offset_list
