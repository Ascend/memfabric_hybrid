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

#ifndef MEMFABRIC_HYBRID_HYBM_DATA_OP_FACTORY_H
#define MEMFABRIC_HYBRID_HYBM_DATA_OP_FACTORY_H

#include "hybm_transport_manager.h"
#include "hybm_data_operator.h"

namespace ock {
namespace mf {
class DataOperatorFactory {
public:
    static DataOperatorPtr CreateSdmaDataOperator();
    static DataOperatorPtr CreateDevRdmaDataOperator(uint32_t rankId, const transport::TransManagerPtr &tm);
    static DataOperatorPtr CreateHostRdmaDataOperator(uint32_t rankId, const transport::TransManagerPtr &tm);
};
} // namespace mf
} // namespace ock

#endif // MEMFABRIC_HYBRID_HYBM_DATA_OP_FACTORY_H
