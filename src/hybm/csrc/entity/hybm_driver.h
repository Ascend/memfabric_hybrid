/*
* Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
*/
#ifndef HYBM_DRIVER_H
#define HYBM_DRIVER_H
std::string GetDriverVersionPath(const std::string &driverEnvStr, const std::string &keyStr);
std::string LoadDriverVersionInfoFile(const std::string &realName, const std::string &keyStr);
std::string CastDriverVersion(const std::string &driverEnv);
int32_t GetValueFromVersion(const std::string &ver, std::string key);
#endif
