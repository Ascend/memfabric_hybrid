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
#include "hybm_data_op_sdma.h"
#include "hybm_data_op_device_rdma.h"
#include "hybm_data_op_host_rdma.h"
#include "hybm_data_op_factory.h"

namespace ock {
namespace mf {
DataOperatorPtr DataOperatorFactory::CreateSdmaDataOperator()
{
    return std::make_shared<HostDataOpSDMA>();
}

DataOperatorPtr DataOperatorFactory::CreateDevRdmaDataOperator(uint32_t rankId, const transport::TransManagerPtr &tm)
{
    return std::make_shared<DataOpDeviceRDMA>(rankId, tm);
}

DataOperatorPtr DataOperatorFactory::CreateHostRdmaDataOperator(uint32_t rankId, const transport::TransManagerPtr &tm)
{
    return std::make_shared<HostDataOpRDMA>(rankId, tm);
}
} // namespace mf
} // namespace ock