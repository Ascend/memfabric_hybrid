
# -*- coding: utf-8 -*-
# Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.

import torch
from examples.verl.model_adaptor.adaptor import ModelAdatpor
from examples.verl.utils import get_prefix_name
from examples.verl.config_interface import (
    build_param_partition_config,
    Engine,
    get_moe_members
)


class QwenAdaptor(ModelAdatpor):
    def __init__(self, train_config, param_name_mapping=None, tp_partition_dict=None):
        self.param_name_mapping = {
            "embedding.word_embeddings": "model.embed_tokens",
            "word_embeddings": "embed_tokens",
            "self_attention.linear_qkv.weight": "self_attn.qkv_proj.weight",
            "self_attention.linear_qkv.bias": "self_attn.qkv_proj.bias",
            "self_attention.linear_proj.weight": "self_attn.o_proj.weight",
            "self_attention.linear_qkv.layer_norm_weight": "input_layernorm.weight",
            "final_layernorm": "norm",
            "output_layer": "lm_head",
            "self_attention.q_layernorm.weight": "self_attn.q_norm.weight",
            "self_attention.k_layernorm.weight": "self_attn.k_norm.weight"
        }
        self.tp_partition_dict = {
            'word_embeddings': 0,
            'output_layer': 0,
            'self_attention.linear_proj.weight': 1,
            'self_attention.linear_qkv.weight': 0,
            'self_attention.q_layernorm.weight': -1,
            'self_attention.k_layernorm.weight': -1,
            'self_attention.linear_proj.bias': 0,
            'self_attention.linear_qkv.bias': 0,
            'self_attention.q_layernorm.bias': -1,
            'self_attention.k_layernorm.bias': -1,
        }
        super().__init__(train_config, param_name_mapping, tp_partition_dict)
        self.fused_ep_param_name = {}
        self.fused_tp_param_name = {"mlp.gate_up_proj.weight": 100, "mlp.linear_fc1.weight": 100}
        self.fused_tp_shared_expert_param_name = {}
        self.fused_meta_params = {}
        self.param_config = {}

    def build_param_name_mapping(self, param_name_mapping):
        if param_name_mapping is None:
            self.param_name_mapping.update({
                    "mlp.linear_fc1.weight": "mlp.gate_up_proj.weight",
                    "mlp.linear_fc1.layer_norm_weight": "post_attention_layernorm.weight",
                    "mlp.linear_fc2.weight": "mlp.down_proj.weight",
            })
        else:
            self.param_name_mapping = param_name_mapping

    def build_tp_partition_dict(self, tp_partition_dict):
        if tp_partition_dict is None:
            self.tp_partition_dict.update({
                'mlp.linear_fc1.weight': 0,
                'mlp.linear_fc2.weight': 1,
                'mlp.linear_fc1.bias': 0,
                'mlp.linear_fc2.bias': 0,
            })
        else:
            self.tp_partition_dict = tp_partition_dict

    def build_param_config(self, model: torch.nn.Module, train_tensor_world_size, infer_tensor_world_size, engine_type):
        param_config = {}
        module_tp_partition_dict = self.tp_partition_dict

        for name, param in model.named_parameters():
            prefix_name = get_prefix_name(name)
            is_valid_registry = param_config.get(prefix_name, True)
            if is_valid_registry:
                tp_partition = ep_partition = False
                shard_dim = -1
                if module_tp_partition_dict.get(prefix_name, -1) != -1:  # -1 means no TP Slice(default)
                    tp_partition = True
                    shard_dim = module_tp_partition_dict.get(prefix_name, -1)
                if "bias" in prefix_name:
                    tp_partition = True
                    shard_dim = 0
                if prefix_name in get_moe_members() or any(s in prefix_name for s in self.fused_ep_param_name.keys()):
                    ep_partition = True

                fused, fused_size = self.get_fused_info(
                    prefix_name, 
                    train_tensor_world_size, 
                    infer_tensor_world_size, 
                    engine_type
                    )
                partition_config = build_param_partition_config(
                    param_size=param.size(),
                    param_ndim=param.ndim,
                    tp_partition=tp_partition,
                    ep_partition=ep_partition,
                    shard_dim=shard_dim,
                    fused=fused,
                    fused_size=fused_size
                )
                param_config.update({prefix_name: partition_config})

        return param_config

    def build_infer_param_config(
        self,
        model: torch.nn.Module,
        train_tensor_world_size,
        infer_tensor_world_size,
        train_param_config
    ):
        param_config = {}
        for name, param in model.named_parameters():
            train_name = self.rewrite(name)
            prefix_name = get_prefix_name(train_name)
            fused_size = 0
            if prefix_name in train_param_config:
                if train_param_config[prefix_name].fused:
                    train_tp = train_tensor_world_size
                    infer_tp = infer_tensor_world_size
                    fused_size = [int(x * train_tp / infer_tp) for x in train_param_config[prefix_name].fused_size]
                partition_config = build_param_partition_config(
                    param_size=param.size(),
                    param_ndim=param.ndim,
                    tp_partition=train_param_config[prefix_name].tp_partition,
                    ep_partition=train_param_config[prefix_name].ep_partition,
                    shard_dim=train_param_config[prefix_name].shard_dim,
                    fused=train_param_config[prefix_name].fused,
                    fused_size=fused_size
                )
            else:
                tp_partition = True
                ep_partition = False
                shard_dim = 0
                if "norm" in name:
                    tp_partition = ep_partition = False
                    shard_dim = -1

                partition_config = build_param_partition_config(
                    param_size=param.size(),
                    param_ndim=param.ndim,
                    tp_partition=tp_partition,
                    ep_partition=ep_partition,
                    shard_dim=shard_dim,
                    fused=False,
                    fused_size=0
                )
            param_config[get_prefix_name(name)] = partition_config

        return param_config

    def get_fused_info(self, module_name: str, train_tensor_world_size, infer_tensor_world_size, engine_type):
        if engine_type == Engine.MEGATRON:
            tp = train_tensor_world_size
        else:
            tp = infer_tensor_world_size
        for name in self.fused_tp_param_name.keys():
            if name in module_name:
                return True, [self.train_config.ffn_hidden_size // tp, self.train_config.ffn_hidden_size // tp]
        return False, None

    def rewrite(self, name):
        infer_name = name
        train_name = infer_name.replace("model", "decoder")
        for infer_name, train_name in self.i2t_param_name_mapping.items():
            train_name = train_name.replace(f"{infer_name}", f"{train_name}")
        return train_name


class QwenMoeAdaptor(QwenAdaptor):
    def __init__(self, train_config, param_name_mapping=None, tp_partition_dict=None):
        super().__init__(train_config, param_name_mapping, tp_partition_dict)

    def build_param_name_mapping(self, param_name_mapping):
        if param_name_mapping is None:
            self.param_name_mapping.update({
                "mlp.router.weight": "mlp.gate.weight",
                "pre_mlp_layernorm.weight": "post_attention_layernorm.weight",
                "mlp.experts.linear_fc1.weight": "mlp.experts.w13_weight", # train 的参数需要合并
                "mlp.experts.linear_fc2.weight": "mlp.experts.w2_weight",
            })
        else:
            self.param_name_mapping = param_name_mapping

    def build_tp_partition_dict(self, tp_partition_dict):
        if tp_partition_dict is not None:
            self.tp_partition_dict = tp_partition_dict
