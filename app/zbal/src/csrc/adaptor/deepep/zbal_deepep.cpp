/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ZBAL is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include <cmath>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/pytypes.h>
#include <pybind11/stl.h>
#include <torch_npu/csrc/core/npu/NPUStream.h>
#include <torch_npu/csrc/core/npu/NPUEvent.h>
#include <torch_npu/csrc/framework/OpCommand.h>
#include <torch_npu/csrc/npu/Event.h>
#include "zbal_common_includes.h"
#include "zbal_operations.h"
#include "dl_cann_api.h"

#include "zbal_deepep.h"

namespace zbal {
namespace adaptor {
namespace deep_ep {
constexpr int PADDING_SIZE = 1;
constexpr size_t COMM_NAME_LEN = 128;
constexpr int A2_MAX_HCCS_PEERS = 8;
int g_magicVal = 0;

using namespace underapi;
using namespace zbal;

const zbal_tensor_info_t transfer_tensor_info(const torch::Tensor &ori_tensor, const int rank = 0,
                                              std::string name = "")
{
    (void)rank;
    (void)name;
    zbal_tensor_info_t result{};

    result.data = const_cast<void *>(ori_tensor.data_ptr());
    auto at_type = ori_tensor.scalar_type();
    switch (at_type) {
        case at::ScalarType::Char:
            result.dataType = ZBAL_DATA_TYPE_INT8;
            break;
        case at::ScalarType::Short:
            result.dataType = ZBAL_DATA_TYPE_INT16;
            break;
        case at::ScalarType::Int:
            result.dataType = ZBAL_DATA_TYPE_INT32;
            break;
        case at::ScalarType::Half:
            result.dataType = ZBAL_DATA_TYPE_FP16;
            break;
        case at::ScalarType::Long:
            result.dataType = ZBAL_DATA_TYPE_INT64;
            break;
        case at::ScalarType::Bool:
            result.dataType = ZBAL_DATA_TYPE_UINT8;
            break;
        case at::ScalarType::Float:
            result.dataType = ZBAL_DATA_TYPE_FP32;
            break;
        case at::ScalarType::Double:
            result.dataType = ZBAL_DATA_TYPE_FP64;
            break;
        case at::ScalarType::BFloat16:
            result.dataType = ZBAL_DATA_TYPE_BFP16;
            break;
        default:
            throw std::runtime_error("Unsupported torch scalar type.");
    }

    const auto &sizes = ori_tensor.sizes();
    result.dim = static_cast<uint16_t>(sizes.size());
    if (result.dim > 25) {
        throw std::runtime_error("Tensor dimension exceeds maximum supported (25).");
    }
    for (size_t i = 0; i < sizes.size(); ++i) {
        result.shape[i] = static_cast<uint16_t>(sizes[i]);
    }
    return result;
}

Buffer::Buffer(int rank, int num_ranks, int64_t num_nvl_bytes, int64_t num_rdma_bytes, bool low_latency_mode,
               std::string moe_group_name)
    : rank(rank), num_ranks(num_ranks), num_nvl_bytes(num_nvl_bytes), num_rdma_bytes(num_rdma_bytes),
      low_latency_mode(low_latency_mode), moe_group_name(moe_group_name)
{
    ZBAL_ASSERT_S(0 <= rank and rank < num_ranks, "rank check failed:", Z_INVALID_VALUE);
    ZBAL_CHECK_S(DlCannApi::AclrtGetDevice(&device_id) == ACL_SUCCESS, "get device_id failed");
    ZBAL_CHECK_S(!moe_group_name.empty() and moe_group_name.size() < COMM_NAME_LEN, "moe_group_name check failed:");

    comm_ = zbal_comm_get_by_name(moe_group_name.c_str());
    std::cout << "[init] rank:" << rank << ", group_name: " << moe_group_name << std::endl;
    ZBAL_CHECK_S(comm_ != nullptr, "get zbal_comm failed");

    soc_version = op::GetCurrentPlatformInfo().GetSocVersion();
    num_rdma_ranks = 1;
    num_nvl_ranks = num_ranks;
    rdma_rank = rank;
    nvl_rank = rank;
    if (soc_version == op::SocVersion::ASCEND910B) {
        ZBAL_ASSERT_S(num_ranks < A2_MAX_HCCS_PEERS || num_ranks % A2_MAX_HCCS_PEERS == 0,
                      "num_ranks check failed:", Z_INVALID_VALUE);
        num_rdma_ranks = std::max(1, num_ranks / A2_MAX_HCCS_PEERS);
        num_nvl_ranks = std::min(num_ranks, A2_MAX_HCCS_PEERS);
        rdma_rank = rank / A2_MAX_HCCS_PEERS;
        nvl_rank = rank % A2_MAX_HCCS_PEERS;
        num_max_hccs_peers = A2_MAX_HCCS_PEERS;
    } else {
        ZBAL_ASSERT_S(num_ranks < static_cast<int>(ZBAL_MAX_RANK_SIZE) || num_ranks % ZBAL_MAX_RANK_SIZE == 0,
                      "num_ranks check failed:", Z_INVALID_VALUE);
        num_max_hccs_peers = ZBAL_MAX_RANK_SIZE;
    }
}

Buffer::~Buffer() noexcept(false) {}

bool Buffer::is_available() const
{
    return available;
}

bool Buffer::is_internode_available() const
{
    // Current version does not support internode
    return is_available() and num_ranks > num_max_hccs_peers;
}

int Buffer::get_num_rdma_ranks() const
{
    return num_rdma_ranks;
}

int Buffer::get_rdma_rank() const
{
    return rdma_rank;
}

void Buffer::clean_low_latency_buffer(int num_max_dispatch_tokens_per_rank, int hidden, int num_experts)
{
    (void)num_max_dispatch_tokens_per_rank;
    (void)hidden;
    (void)num_experts;
    return;
}

std::tuple<torch::Tensor, std::optional<torch::Tensor>, torch::Tensor, torch::Tensor, std::optional<EventHandle>>
Buffer::get_dispatch_layout(const torch::Tensor &topk_idx, int num_experts, std::optional<EventHandle> &previous_event,
                            bool async, bool allocate_on_comm_stream)
{
    (void)async;
    (void)previous_event;
    (void)allocate_on_comm_stream;
    ZBAL_CHECK_S(topk_idx.dim() == 2, "Layout: check topk_idx dim failed");
    ZBAL_CHECK_S(topk_idx.is_contiguous(), "Layout: check topk_idx contiguous failed");
    ZBAL_CHECK_S(num_experts > 0, "Layout: check num_experts failed:", num_experts);

    int num_tokens = static_cast<int>(topk_idx.size(0));
    int num_topk = static_cast<int>(topk_idx.size(1));
    auto device = topk_idx.device();
    auto num_tokens_per_expert = at::empty({num_experts}, at::dtype(at::kInt).device(device));
    auto num_tokens_per_rank = at::empty({num_ranks}, at::dtype(at::kInt).device(device));
    auto is_token_in_rank = at::empty({num_tokens, num_ranks}, at::dtype(at::kInt).device(device));
    auto send_token_idx = at::empty({num_tokens, num_topk}, at::dtype(at::kInt).device(device));
    auto num_tokens_per_rdma_rank = std::optional<torch::Tensor>();
    if (is_internode_available()) {
        num_tokens_per_rdma_rank = at::empty({num_rdma_ranks}, dtype(at::kInt).device(device));
    }
    int blocks = 50;
    auto notify_send_data = at::empty({num_experts * blocks}, at::dtype(at::kInt).device(device));
    auto acl_stream = c10_npu::getCurrentNPUStream().stream(false);
    int64_t flags = 0;

    // tensor to zbal_tensor_info_t
    auto topk_idx_info = transfer_tensor_info(topk_idx);
    auto num_tokens_per_rank_info = transfer_tensor_info(num_tokens_per_rank);
    auto num_tokens_per_expert_info = transfer_tensor_info(num_tokens_per_expert);
    auto is_token_in_rank_info = transfer_tensor_info(is_token_in_rank);
    auto send_token_idx_info = transfer_tensor_info(send_token_idx);
    auto notify_send_data_info = transfer_tensor_info(notify_send_data);

    std::function<int()> acl_call;
    acl_call = [this, topk_idx_info, num_tokens, num_experts, num_topk, num_tokens_per_rank_info,
                num_tokens_per_expert_info, is_token_in_rank_info, send_token_idx_info, notify_send_data_info,
                acl_stream, flags]() -> int {
        auto api_ret = zbal_dispatch_normal_layout(
            &topk_idx_info, num_tokens, num_experts, num_topk, &num_tokens_per_rank_info, &num_tokens_per_expert_info,
            &is_token_in_rank_info, &send_token_idx_info, &notify_send_data_info, this->comm_, acl_stream, flags);
        return api_ret;
    };
    at_npu::native::OpCommand::RunOpApiV2("zbal_dispatch_normal_layout", acl_call);

    std::optional<EventHandle> event;
    this->send_token_idx = send_token_idx;

    return {num_tokens_per_rank, num_tokens_per_rdma_rank, num_tokens_per_expert, is_token_in_rank, event};
}

std::tuple<at::Tensor, std::optional<at::Tensor>, std::optional<at::Tensor>, std::optional<at::Tensor>,
           std::vector<int>, at::Tensor, at::Tensor, std::optional<EventHandle>>
Buffer::intranode_dispatch(const at::Tensor &x, const std::optional<at::Tensor> &x_scales,
                           const std::optional<at::Tensor> &topk_idx, const std::optional<at::Tensor> &topk_weights,
                           const std::optional<at::Tensor> &num_tokens_per_rank, const at::Tensor &is_token_in_rank,
                           const std::optional<at::Tensor> &num_tokens_per_expert, int num_worst_tokens,
                           const Config &config, std::optional<EventHandle> &previous_event, bool async,
                           bool allocate_on_comm_stream, bool use_quant)
{
    (void)x_scales;
    (void)topk_weights;
    (void)is_token_in_rank;
    (void)num_worst_tokens;
    (void)config;
    (void)previous_event;
    (void)async;
    (void)allocate_on_comm_stream;
    auto device = x.device();

    zbal_quant_mode_t quant_mode = use_quant ? QUANT_BF16_2_INT8 : NO_QUANT;
    auto recv_topk_idx = std::optional<at::Tensor>();
    auto recv_topk_weights = std::optional<at::Tensor>();
    // Wait streams
    std::optional<EventHandle> event;

    ZBAL_CHECK_S(num_tokens_per_rank.has_value(), "num_tokens_per_rank is empty");
    ZBAL_CHECK_S(num_tokens_per_expert.has_value(), "num_tokens_per_expert is empty");

    // Type checks
    ZBAL_CHECK_S(num_tokens_per_expert->scalar_type() == at::kInt, "num_tokens_per_expert scalar type is not kInt");
    ZBAL_CHECK_S(num_tokens_per_rank->scalar_type() == at::kInt, "num_tokens_per_rank scalar type is not kInt");

    // Shape and contiguous checks
    ZBAL_CHECK_S(x.dim() == 2 and x.is_contiguous(), "x dim not 2 or not comtiguous");
    ZBAL_CHECK_S(num_tokens_per_expert->dim() == 1 and num_tokens_per_expert->is_contiguous(),
                 "num_tokens_per_expert check failed");
    ZBAL_CHECK_S(num_tokens_per_expert->size(0) % num_ranks == 0, "num_tokens_per_expert check failed");
    ZBAL_CHECK_S(num_tokens_per_rank->dim() == 1 and num_tokens_per_rank->is_contiguous(),
                 "num_tokens_per_rank check failed");
    ZBAL_CHECK_S(num_tokens_per_rank->size(0) == num_ranks, "num_tokens_per_rank check failed");

    auto num_tokens = static_cast<int>(x.size(0)), hidden = static_cast<int>(x.size(1));
    auto num_experts = static_cast<int>(num_tokens_per_expert->size(0));
    ZBAL_CHECK_S(num_experts > 0, "num_experts check failed");
    auto num_local_experts = static_cast<int>(num_experts / num_ranks);
    auto new_num_tokens_per_expert = num_tokens_per_expert.value();
    auto send_token_idx = this->send_token_idx;

    // Top-k checks
    int num_topk = 0;
    ZBAL_CHECK_S(topk_idx.has_value(), "topk_idx is empty");
    if (topk_idx.has_value()) {
        num_topk = static_cast<int>(topk_idx->size(1));
        (void)num_topk;
        ZBAL_CHECK_S(num_tokens == topk_idx->size(0), "topk_idx check failed");
        ZBAL_CHECK_S(topk_idx->dim() == 2 and topk_idx->is_contiguous(), "topk_idx check failed");
    }
    auto expert_ids = topk_idx.value().to(at::kInt);
    int topk_num = static_cast<int>(expert_ids.size(1));

    std::vector<int> num_recv_tokens_per_expert_list;
    // indicates the value type of the output num_recv_tokens_per_expert_list, with a range of [0, 1]
    // 0 means the prefix sum of the number of tokens received by each expert;
    // 1 means the number of tokens received by each expert (default)
    int expert_token_nums_type = get_value_from_env("MOE_EXPERT_TOKEN_NUMS_TYPE", 1);
    ZBAL_CHECK_S(expert_token_nums_type == 1 or expert_token_nums_type == 0, "expert_token_nums_type is invalid");

    int send_per_group = 1; // (send_to_expert_num,)
    int send_count = send_per_group * num_experts;
    auto recv_data = torch::empty({num_ranks, num_experts}, at::dtype(at::kInt).device(device));
    auto recv_tokens_per_expert = torch::empty({num_local_experts}, at::dtype(at::kLong).device(device));
    auto put_offset = torch::empty({num_experts, num_ranks}, at::dtype(at::kInt).device(device));
    auto balance_matrix = torch::empty({num_ranks, num_ranks * 2}, at::dtype(at::kInt).device(device));
    auto total_recv_token = torch::empty({1}, at::dtype(at::kInt).device(device));
    auto acl_stream = c10_npu::getCurrentNPUStream().stream(false);
    int64_t flags = 0;

    // tensor to zbal_tensor_info_t
    auto num_tokens_per_expert_info = transfer_tensor_info(new_num_tokens_per_expert);
    auto recv_data_info = transfer_tensor_info(recv_data);
    auto recv_tokens_per_expert_info = transfer_tensor_info(recv_tokens_per_expert);
    auto put_offset_info = transfer_tensor_info(put_offset);
    auto balance_matrix_info = transfer_tensor_info(balance_matrix);
    auto total_recv_token_info = transfer_tensor_info(total_recv_token);

    auto x_info = transfer_tensor_info(x);
    auto expert_ids_info = transfer_tensor_info(expert_ids);
    auto send_token_idx_info = transfer_tensor_info(send_token_idx);

    // call notify
    std::function<int()> acl_call_notify;
    acl_call_notify = [this, num_tokens_per_expert_info, send_count, topk_num, recv_data_info, total_recv_token_info,
                       recv_tokens_per_expert_info, put_offset_info, balance_matrix_info, acl_stream, flags]() -> int {
        auto api_ret = zbal_dispatch_normal_notify(
            &num_tokens_per_expert_info, send_count, topk_num, &recv_data_info, &total_recv_token_info,
            &recv_tokens_per_expert_info, &put_offset_info, &balance_matrix_info, this->comm_, acl_stream, flags);
        return api_ret;
    };
    at_npu::native::OpCommand::RunOpApiV2("zbal_dispatch_normal_notify", acl_call_notify);

    int total_recv_cnt = total_recv_token.item<int>();
    int num_recv_tokens = (total_recv_cnt == 0) ? 1 : total_recv_cnt;
    auto expandx_out = use_quant ? torch::empty({num_recv_tokens, hidden}, at::dtype(at::kChar).device(device))
                                 : torch::empty({num_recv_tokens, hidden}, x.options());
    auto dynamic_scales_out = use_quant ? torch::empty({num_recv_tokens}, at::dtype(at::kFloat).device(device))
                                        : torch::empty({1}, at::dtype(at::kFloat).device(device));
    // tensor to zbal_tensor_info_t
    auto expandx_out_info = transfer_tensor_info(expandx_out);
    auto dynamic_scales_out_info = transfer_tensor_info(dynamic_scales_out);

    // call dispatch
    std::function<int()> acl_call_dispatch;
    acl_call_dispatch = [this, x_info, expert_ids_info, send_token_idx_info, put_offset_info, balance_matrix_info,
                         num_experts, quant_mode, expandx_out_info, dynamic_scales_out_info, acl_stream,
                         flags]() -> int {
        auto api_ret = zbal_dispatch_normal(&x_info, &expert_ids_info, &send_token_idx_info, &put_offset_info,
                                            &balance_matrix_info, num_experts, quant_mode, &expandx_out_info,
                                            &dynamic_scales_out_info, this->comm_, acl_stream, flags);
        return api_ret;
    };
    at_npu::native::OpCommand::RunOpApiV2("zbal_dispatch_normal", acl_call_dispatch);

    auto recv_token_per_exp_cpu = recv_tokens_per_expert.to(at::kCPU);
    auto recv_token_per_exp_ptr = recv_token_per_exp_cpu.data_ptr<int64_t>();

    int token_cnt = 0;
    for (int local_e = 0; local_e < num_local_experts; ++local_e) {
        int current_tokens = static_cast<int>(recv_token_per_exp_ptr[local_e]);
        token_cnt = (expert_token_nums_type == 0) ? token_cnt + current_tokens : current_tokens;
        num_recv_tokens_per_expert_list.emplace_back(token_cnt);
    }
    // Return values
    return {expandx_out, dynamic_scales_out, recv_topk_idx, recv_topk_weights, num_recv_tokens_per_expert_list,
            put_offset,  balance_matrix,     event};
}

std::tuple<torch::Tensor, std::optional<torch::Tensor>, std::optional<EventHandle>>
Buffer::intranode_combine(const torch::Tensor &x, const torch::Tensor &topk_idx,
                          const std::optional<torch::Tensor> &topk_weights, const torch::Tensor &put_offset,
                          const torch::Tensor &balance_matrix, std::optional<EventHandle> &previous_event, bool async,
                          bool allocate_on_comm_stream)
{
    (void)previous_event;
    (void)async;
    (void)allocate_on_comm_stream;
    ZBAL_CHECK_S(x.dim() == 2 and x.is_contiguous(), "x dim not 2 or not comtiguous");
    ZBAL_CHECK_S(topk_idx.dim() == 2 and topk_idx.is_contiguous(), "topk_idx dim not 2 or not comtiguous");
    ZBAL_CHECK_S(balance_matrix.dim() == 2 and balance_matrix.is_contiguous(),
                 "balance_matrix dim not 2 or not comtiguous");
    auto recv_x = x;
    auto expert_ids = topk_idx.to(at::kInt);

    const int num_topk = static_cast<int>(expert_ids.size(1));
    const int hidden = static_cast<int>(recv_x.size(1));

    ZBAL_CHECK_S(topk_weights.has_value(), "topk_weights is empty");
    if (topk_weights.has_value()) {
        ZBAL_CHECK_S(topk_weights->dim() == 2 and topk_weights->is_contiguous(),
                     "topk_weights dim not 2 or not comtiguous");
        ZBAL_CHECK_S(num_topk == topk_weights->size(1), "topk_weights shape check failed");
        ZBAL_CHECK_S(topk_weights->scalar_type() == at::kFloat, "topk_weights scalar type check failed");
    }
    auto expert_scales = topk_weights.value();
    uint16_t moe_expert_number = static_cast<uint16_t>(put_offset.size(0));
    auto send_token_idx = this->send_token_idx;

    auto combined_x = torch::empty({expert_scales.size(0), hidden}, x.options());
    std::optional<torch::Tensor> recv_topk_weights;
    std::optional<EventHandle> event;
    auto acl_stream = c10_npu::getCurrentNPUStream().stream(false);
    int64_t flags = 0;

    // tensor to zbal_tensor_info_t
    auto recv_x_info = transfer_tensor_info(recv_x);
    auto ep_send_counts_info = transfer_tensor_info(put_offset);
    auto expert_scales_info = transfer_tensor_info(expert_scales);
    auto expert_ids_info = transfer_tensor_info(expert_ids);
    auto send_token_idx_info = transfer_tensor_info(send_token_idx);
    auto balance_matrix_info = transfer_tensor_info(balance_matrix);
    auto combined_x_info = transfer_tensor_info(combined_x);

    // call combine
    std::function<int()> acl_call;
    acl_call = [this, recv_x_info, ep_send_counts_info, expert_scales_info, expert_ids_info, send_token_idx_info,
                balance_matrix_info, moe_expert_number, combined_x_info, acl_stream, flags]() -> int {
        auto api_ret = zbal_combine_normal(&recv_x_info, &ep_send_counts_info, &expert_scales_info, &expert_ids_info,
                                           &send_token_idx_info, &balance_matrix_info, moe_expert_number,
                                           &combined_x_info, this->comm_, acl_stream, flags);
        return api_ret;
    };
    at_npu::native::OpCommand::RunOpApiV2("zbal_combine_normal", acl_call);

    return {combined_x, recv_topk_weights, event};
}

std::tuple<at::Tensor, std::optional<at::Tensor>, at::Tensor, at::Tensor, at::Tensor, std::optional<EventHandle>,
           std::optional<std::function<void()>>>
Buffer::low_latency_dispatch(const at::Tensor &x, const at::Tensor &topk_idx,
                             const std::optional<at::Tensor> &cumulative_local_expert_recv_stats,
                             int64_t num_max_dispatch_tokens_per_rank, int64_t num_experts, bool use_fp8,
                             bool round_scale, bool use_ue8m0, bool async, bool return_recv_hook)
{
    (void)cumulative_local_expert_recv_stats;
    (void)round_scale;
    (void)use_ue8m0;
    (void)async;
    (void)return_recv_hook;
    g_magicVal += 1;
    this->is_padding = false;
    at::Tensor new_x = x;
    this->new_topk_idx = topk_idx;
    if (topk_idx.size(0) < PADDING_SIZE) {
        this->is_padding = true;
        this->padding_cnt = PADDING_SIZE - topk_idx.size(0);
        std::vector<at::Tensor> x_blocks;
        std::vector<at::Tensor> topk_blocks;
        if (topk_idx.size(0) != 0) {
            x_blocks.emplace_back(x);
            topk_blocks.emplace_back(topk_idx);
        } else {
            this->ori_x = x.clone();
        }
        int topk = static_cast<int>(new_topk_idx.size(1));
        for (int i = 0; i < this->padding_cnt; i++) {
            at::Tensor tmp_x = torch::ones({1, x.size(1)}, x.options());
            at::Tensor tmp_topk = torch::arange(0, topk, topk_idx.options()).reshape({1, topk});
            x_blocks.emplace_back(tmp_x);
            topk_blocks.emplace_back(tmp_topk);
        }
        new_x = torch::cat(x_blocks, 0);
        this->new_topk_idx = torch::cat(topk_blocks, 0);
    }

    auto expert_ids = new_topk_idx;
    int shared_expert_rank_num = get_value_from_env("MOE_SHARED_EXPERT_RANK_NUM", 0);

    auto num_tokens = static_cast<int>(new_x.size(0)), hidden = static_cast<int>(new_x.size(1));
    auto num_topk = static_cast<int>(new_topk_idx.size(1));
    auto num_local_experts = num_experts / (num_ranks - shared_expert_rank_num);
    auto acl_stream = c10_npu::getCurrentNPUStream().stream(false);

    int64_t global_bs = std::max(new_topk_idx.size(0), num_max_dispatch_tokens_per_rank) * num_ranks;
    auto num_max_tokens = 0;
    if (rank < shared_expert_rank_num) {
        num_max_tokens = global_bs / shared_expert_rank_num;
        num_local_experts = 1;
    } else { // moe expert
        num_max_tokens = global_bs * num_local_experts;
    }
    int localMagic = g_magicVal;
    auto max_size = std::max(num_tokens * num_topk, num_max_tokens * 128);
    // Allocate packed tensors
    auto device = x.device();
    auto packed_recv_x =
        at::empty({num_max_tokens, hidden}, new_x.options().dtype(use_fp8 ? at::kChar : at::kBFloat16));
    auto packed_recv_x_scales = at::empty({num_max_tokens}, at::dtype(at::kFloat).device(device));
    auto expandIdx = at::empty({max_size}, at::dtype(at::kInt).device(device));

    at::Tensor ep_recv_count =
        at::empty({num_experts * num_ranks}, at::dtype(at::kInt).device(device)); // A2 non-layered / A3
    auto tp_recv_count = at::empty({1}, at::dtype(at::kInt).device(device));
    auto packed_recv_count = at::empty({num_local_experts}, at::dtype(at::kLong).device(device));

    int enable_neg_one = get_value_from_env("MOE_ENABLE_TOPK_NEG_ONE", 0);
    (void)enable_neg_one;
    int64_t quant_mode = use_fp8 ? QUANT_BF16_2_INT8 : NO_QUANT;
    int64_t shared_expert_num = 1;
    int outType = get_value_from_env("MOE_EXPERT_TOKEN_NUMS_TYPE", 1);
    int64_t expert_token_nums_type = outType;
    int64_t flags = 0;

    // Wait streams
    std::optional<EventHandle> event;
    auto putOffset = at::empty({num_experts * num_ranks}, at::dtype(at::kInt).device(device));
    auto putOffsetStatus = at::empty({num_experts * num_ranks}, at::dtype(at::kFloat).device(device));
    // tensor to zbal_tensor_info_t
    auto new_x_info = transfer_tensor_info(new_x);
    auto expert_ids_info = transfer_tensor_info(expert_ids);
    auto expandx_out_info = transfer_tensor_info(packed_recv_x);
    auto expert_token_nums_out_info = transfer_tensor_info(packed_recv_count);
    auto ep_recv_count_out_info = transfer_tensor_info(ep_recv_count);
    auto expandIdx_out_info = transfer_tensor_info(expandIdx);
    auto dynamic_scales_out_info = transfer_tensor_info(packed_recv_x_scales);
    auto putOffset_info = transfer_tensor_info(putOffset);
    auto putOffsetStatus_info = transfer_tensor_info(putOffsetStatus);

    // call low_latency_dispatch
    std::function<int()> acl_call_low_latency_dispatch;
    acl_call_low_latency_dispatch = [this, new_x_info, expert_ids_info, num_experts, shared_expert_num,
                                     shared_expert_rank_num, quant_mode, global_bs, localMagic, expert_token_nums_type,
                                     expandx_out_info, dynamic_scales_out_info, expandIdx_out_info,
                                     expert_token_nums_out_info, ep_recv_count_out_info, putOffset_info,
                                     putOffsetStatus_info, acl_stream, flags]() -> int {
        auto api_ret = zbal_dispatch_low_latency(
            &new_x_info, &expert_ids_info, num_experts, shared_expert_num, shared_expert_rank_num, quant_mode,
            global_bs, localMagic, expert_token_nums_type, &expandx_out_info, &dynamic_scales_out_info,
            &expandIdx_out_info, &expert_token_nums_out_info, &ep_recv_count_out_info, &putOffset_info,
            &putOffsetStatus_info, this->comm_, acl_stream, flags);
        return api_ret;
    };
    at_npu::native::OpCommand::RunOpApiV2("zbal_dispatch_low_latency", acl_call_low_latency_dispatch);

    // Return values
    return {packed_recv_x, packed_recv_x_scales,        packed_recv_count, expandIdx, ep_recv_count,
            event,         std::function<void()>([] {})};
}

std::tuple<at::Tensor, std::optional<EventHandle>, std::optional<std::function<void()>>>
Buffer::low_latency_combine(const at::Tensor &x, const at::Tensor &topk_idx, const at::Tensor &topk_weights,
                            const at::Tensor &src_info, const at::Tensor &layout_range,
                            int64_t num_max_dispatch_tokens_per_rank, int64_t num_experts,
                            const at::Tensor &packed_recv_count, bool zero_copy, bool async, bool return_recv_hook,
                            const std::optional<at::Tensor> &out)
{
    (void)num_max_dispatch_tokens_per_rank;
    (void)packed_recv_count;
    (void)zero_copy;
    (void)async;
    (void)return_recv_hook;
    (void)out;
    at::Tensor new_idx = topk_idx;
    at::Tensor new_scales = topk_weights;
    if (this->is_padding) {
        std::vector<at::Tensor> scales_blocks;
        if (this->padding_cnt != PADDING_SIZE) {
            scales_blocks.emplace_back(topk_weights);
        }
        for (int i = 0; i < this->padding_cnt; i++) {
            at::Tensor tmp_scales = torch::zeros({1, topk_weights.size(1)}, topk_weights.options());
            scales_blocks.emplace_back(tmp_scales);
        }
        new_idx = this->new_topk_idx;
        this->new_scales = torch::cat(scales_blocks, 0);
        new_scales = this->new_scales;
    }
    if (this->is_padding) {
        std::vector<at::Tensor> scales_blocks;
        if (this->padding_cnt != PADDING_SIZE) {
            scales_blocks.emplace_back(topk_weights);
        }
        for (int i = 0; i < this->padding_cnt; i++) {
            at::Tensor tmp_scales = torch::zeros({1, topk_weights.size(1)}, topk_weights.options());
            scales_blocks.emplace_back(tmp_scales);
        }
        new_idx = this->new_topk_idx;
        this->new_scales = torch::cat(scales_blocks, 0);
        new_scales = this->new_scales;
    }

    // Tensor checks
    ZBAL_CHECK_S(x.dim() == 2 and x.is_contiguous() and x.scalar_type() == at::kBFloat16, "LL combine x check failed");

    auto device = x.device();
    at::Tensor expand_x = x;
    at::Tensor expert_ids = new_idx;
    at::Tensor expert_idx = src_info;
    at::Tensor ep_send_counts = layout_range;
    at::Tensor expert_scales = new_scales;
    at::Tensor tp_send_counts = at::empty({1}, at::dtype(at::kInt).device(device));
    at::Tensor activation_scale, weight_scale, group_list, expand_scales;
    auto acl_stream = c10_npu::getCurrentNPUStream().stream(false);
    int64_t shared_expert_rank_num = get_value_from_env("MOE_SHARED_EXPERT_RANK_NUM", 0);
    (void)shared_expert_rank_num;
    int64_t flags = 0;

    auto num_combined_tokens = static_cast<int>(new_scales.size(0));
    auto hidden = static_cast<int>(x.size(1));
    at::Tensor shared_expert_x{nullptr};
    at::Tensor combined_x = at::empty({num_combined_tokens, hidden}, x.options());
    std::optional<EventHandle> event;

    // tensor to zbal_tensor_info_t
    auto expand_x_info = transfer_tensor_info(expand_x);
    auto expert_ids_info = transfer_tensor_info(expert_ids);
    auto expert_idx_info = transfer_tensor_info(expert_idx);
    auto ep_send_counts_info = transfer_tensor_info(ep_send_counts);
    auto expert_scales_info = transfer_tensor_info(expert_scales);
    auto combined_x_info = transfer_tensor_info(combined_x);

    // call low_latency_combine
    std::function<int()> acl_call_low_latency_combine;
    acl_call_low_latency_combine = [this, expand_x_info, expert_ids_info, expert_idx_info, ep_send_counts_info,
                                    expert_scales_info, combined_x_info, num_experts, acl_stream, flags]() -> int {
        auto api_ret = zbal_combine_low_latency(&expand_x_info, &expert_ids_info, &expert_idx_info,
                                                &ep_send_counts_info, &expert_scales_info, &combined_x_info,
                                                num_experts, this->comm_, acl_stream, flags);
        return api_ret;
    };
    at_npu::native::OpCommand::RunOpApiV2("zbal_combine_low_latency", acl_call_low_latency_combine);

    if (this->is_padding) {
        if (this->padding_cnt == PADDING_SIZE) {
            combined_x = this->ori_x;
        } else {
            combined_x = combined_x.slice(0, 0, PADDING_SIZE - this->padding_cnt);
        }
        is_padding = false;
    }

    if (this->is_padding) {
        if (this->padding_cnt == PADDING_SIZE) {
            combined_x = this->ori_x;
        } else {
            combined_x = combined_x.slice(0, 0, PADDING_SIZE - this->padding_cnt);
        }
        is_padding = false;
    }

    return {combined_x, event, std::function<void()>([] {})};
}

} // namespace deep_ep
} // namespace adaptor
} // namespace zbal

void pybind11_deepep_adaptor(pybind11::module_ &m)
{
    m.doc() = "DeepEP: an efficient expert-parallel communication library";

    pybind11::class_<zbal::adaptor::deep_ep::Config>(m, "Config")
        .def(pybind11::init<int, int, int, int, int>(), py::arg("num_sms") = 20,
             py::arg("num_max_nvl_chunked_send_tokens") = 6, py::arg("num_max_nvl_chunked_recv_tokens") = 256,
             py::arg("num_max_rdma_chunked_send_tokens") = 6, py::arg("num_max_rdma_chunked_recv_tokens") = 256)
        .def("get_nvl_buffer_size_hint", &zbal::adaptor::deep_ep::Config::get_nvl_buffer_size_hint)
        .def("get_rdma_buffer_size_hint", &zbal::adaptor::deep_ep::Config::get_rdma_buffer_size_hint);
    m.def("get_low_latency_rdma_size_hint", &zbal::adaptor::deep_ep::get_low_latency_rdma_size_hint);

    pybind11::class_<zbal::adaptor::deep_ep::EventHandle>(m, "EventHandle")
        .def(pybind11::init<>())
        .def("current_stream_wait", &zbal::adaptor::deep_ep::EventHandle::current_stream_wait);

    pybind11::class_<zbal::adaptor::deep_ep::Buffer>(m, "Buffer")
        .def(pybind11::init<int, int, int64_t, int64_t, bool, std::string>())
        .def("is_available", &zbal::adaptor::deep_ep::Buffer::is_available)
        .def("get_num_rdma_ranks", &zbal::adaptor::deep_ep::Buffer::get_num_rdma_ranks)
        .def("get_rdma_rank", &zbal::adaptor::deep_ep::Buffer::get_rdma_rank)
        .def("get_dispatch_layout", &zbal::adaptor::deep_ep::Buffer::get_dispatch_layout)
        .def("intranode_dispatch", &zbal::adaptor::deep_ep::Buffer::intranode_dispatch)
        .def("intranode_combine", &zbal::adaptor::deep_ep::Buffer::intranode_combine)
        .def("low_latency_dispatch", &zbal::adaptor::deep_ep::Buffer::low_latency_dispatch)
        .def("low_latency_combine", &zbal::adaptor::deep_ep::Buffer::low_latency_combine)
        .def("clean_low_latency_buffer", &zbal::adaptor::deep_ep::Buffer::clean_low_latency_buffer);
}