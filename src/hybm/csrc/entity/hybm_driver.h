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
#ifndef HYBM_DRIVER_H
#define HYBM_DRIVER_H
std::string GetDriverVersionPath(const std::string &driverEnvStr, const std::string &keyStr);
std::string LoadDriverVersionInfoFile(const std::string &realName, const std::string &keyStr);
std::string CastDriverVersion(const std::string &driverEnv);
int32_t GetValueFromVersion(const std::string &ver, std::string key);
#endif