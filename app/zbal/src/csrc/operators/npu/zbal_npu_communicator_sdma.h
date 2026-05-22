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
#ifndef ZBAL_NPU_COMMUNICATOR_SDMA_H
#define ZBAL_NPU_COMMUNICATOR_SDMA_H

#include "zbal_npu_communicator_base.h"

namespace zbal {
namespace operators {

class NpuCommunicatorSDMA : public NpuCommunicatorBase {
public:
    using NpuCommunicatorBase::NpuCommunicatorBase;

    int32_t Scatter(const void *sendBuff, void *recvBuff, uint64_t data_count, zbal_datatype_t dataType, uint16_t root,
                    aclrtStream stream) noexcept override;
};

} // namespace operators
} // namespace zbal

#endif // ZBAL_NPU_COMMUNICATOR_SDMA_H
