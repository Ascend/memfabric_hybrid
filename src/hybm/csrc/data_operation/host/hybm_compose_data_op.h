/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * MemFabric_Hybrid is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef MEMFABRIC_HYBRID_HYBM_COMPOSE_DATA_OP_H
#define MEMFABRIC_HYBRID_HYBM_COMPOSE_DATA_OP_H

#include <cstdint>
#include <vector>
#include "hybm_entity_tag_info.h"
#include "hybm_data_operator.h"
#include "hybm_transport_manager.h"

namespace ock {
namespace mf {
/**
 * @brief Combine multiple data operators into a single external interface, with the diversity of data operators
 * not exposed to the Entity.
 */
class HostComposeDataOp : public DataOperator {
public:
    HostComposeDataOp(hybm_options options, transport::TransManagerPtr tm, HybmEntityTagInfoPtr tag) noexcept;
    ~HostComposeDataOp() noexcept override;

    Result Initialize() noexcept override;
    void UnInitialize() noexcept override;
    Result DataCopy(hybm_copy_params &params, hybm_data_copy_direction direction,
                    const ExtOptions &options) noexcept override;
    Result BatchDataCopy(hybm_batch_copy_params &params, hybm_data_copy_direction direction,
                         const ExtOptions &options) noexcept override;
    Result DataCopyAsync(hybm_copy_params &params, hybm_data_copy_direction direction,
                         const ExtOptions &options) noexcept override;
    Result Wait(int32_t waitId) noexcept override;

private:
    using DataOperators = std::vector<std::pair<hybm_data_op_type, DataOperatorPtr>>;
    DataOperators GetPrioritedDataOperators(const ExtOptions &options) noexcept;

private:
    const hybm_options options_;
    const transport::TransManagerPtr transport_;
    HybmEntityTagInfoPtr entityTagInfo_;
    DataOperatorPtr sdmaDataOperator_;
    DataOperatorPtr devRdmaDataOperator_;
    DataOperatorPtr hostRdmaDataOperator_;
};
} // namespace mf
} // namespace ock
#endif // MEMFABRIC_HYBRID_HYBM_COMPOSE_DATA_OP_H
