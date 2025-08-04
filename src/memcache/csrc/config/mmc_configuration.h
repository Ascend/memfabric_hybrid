/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */

#pragma once

#include <map>
#include <utility>
#include <vector>

#include "mmc_lock.h"
#include "mmc_ref.h"
#include "mmc_config_validator.h"
#include "mmc_config_convertor.h"
#include "mmc_config_const.h"
#include "mmc_def.h"
#include "mmc_logger.h"
#include "mmc_types.h"

namespace ock {
namespace mmc {
constexpr uint32_t CONF_MUST = 1;
constexpr uint64_t DRAM_SIZE_ALIGNMENT = 2097152; // 2MB

enum class ConfValueType {
    VINT = 0,
    VFLOAT = 1,
    VSTRING = 2,
    VBOOL = 3,
    VUINT64 = 4,
};

class Configuration;
using ConfigurationPtr = MmcRef<Configuration>;

class Configuration : public MmcReferable {
public:
    Configuration() = default;
    ~Configuration() override;

    // forbid copy operation
    Configuration(const Configuration &) = delete;
    Configuration &operator = (const Configuration &) = delete;

    // forbid move operation
    Configuration(const Configuration &&) = delete;
    Configuration &operator = (const Configuration &&) = delete;

    bool LoadFromFile(const std::string &filePath);

    int32_t GetInt(const std::pair<const char *, int32_t> &item);
    float GetFloat(const std::pair<const char *, float> &item);
    std::string GetString(const std::pair<const char *, const char *> &item);
    bool GetBool(const std::pair<const char *, bool> &item);
    uint64_t GetUInt64(const std::pair<const char *, uint64_t> &item);
    std::string GetConvertedValue(const std::string &key);

    void Set(const std::string &key, int32_t value);
    void Set(const std::string &key, float value);
    void Set(const std::string &key, const std::string &value);
    void Set(const std::string &key, bool value);
    void Set(const std::string &key, uint64_t value);

    bool SetWithTypeAutoConvert(const std::string &key, const std::string &value);

    void AddIntConf(const std::pair<std::string, int> &pair, const ValidatorPtr &validator = nullptr,
        uint32_t flag = CONF_MUST);
    void AddStrConf(const std::pair<std::string, std::string> &pair, const ValidatorPtr &validator = nullptr,
        uint32_t flag = CONF_MUST);
    void AddBoolConf(const std::pair<std::string, bool> &pair, const ValidatorPtr &validator = nullptr,
        uint32_t flag = CONF_MUST);
    void AddUInt64Conf(const std::pair<std::string, uint64_t> &pair, const ValidatorPtr &validator = nullptr,
        uint32_t flag = CONF_MUST);
    void AddConverter(const std::string &key, const ConverterPtr &converter);
    void AddPathConf(const std::pair<std::string, std::string> &pair, const ValidatorPtr &validator = nullptr,
        uint32_t flag = CONF_MUST);
    std::vector<std::string> ValidateConf();
    void GetTlsConfig(mmc_tls_config &tlsConfig);

    bool Initialized() const
    {
        return mInitialized;
    }

private:
    void SetValidator(const std::string &key, const ValidatorPtr &validator, uint32_t flag);

    void ValidateOneValueMap(std::vector<std::string> &errors,
        const std::map<std::string, ValidatorPtr> &valueValidator);

    template <class T>
    static void AddValidateError(const ValidatorPtr &validator, std::vector<std::string> &errors, const T &iter)
    {
        if (validator == nullptr) {
            errors.push_back("Failed to validate <" + iter->first + ">, validator is NULL");
            return;
        }
        if (!(validator->Validate(iter->second))) {
            errors.push_back(validator->ErrorMessage());
        }
    }
    void ValidateOneType(const std::string &key, const ValidatorPtr &validator,
        std::vector<std::string> &errors, ConfValueType &vType);

    void ValidateItem(const std::string &itemKey, std::vector<std::string> &errors);

    void LoadConfigurations();

    virtual void LoadDefault() {}

    std::string mConfigPath;

    std::map<std::string, int32_t> mIntItems;
    std::map<std::string, float> mFloatItems;
    std::map<std::string, std::string> mStrItems;
    std::map<std::string, bool> mBoolItems;
    std::map<std::string, uint64_t> mUInt64Items;
    std::map<std::string, std::string> mAllItems;

    std::map<std::string, ConfValueType> mValueTypes;
    std::map<std::string, ValidatorPtr> mValueValidator;
    std::map<std::string, ConverterPtr> mValueConverter;

    std::vector<std::pair<std::string, std::string>> mServiceList;
    std::vector<std::string> mMustKeys;
    std::vector<std::string> mLoadDefaultErrors;

    std::vector<std::string> mPathConfs;
    std::vector<std::string> mExceptPrintConfs;
    std::vector<std::string> mInvalidSetConfs;

    bool mInitialized = false;
    Lock mLock;
};

class MetaServiceConfig final : public Configuration {
public:
    void LoadDefault() override {
        using namespace ConfConstant;
        AddStrConf(OCK_MMC_META_SERVICE_URL, VNoCheck::Create(), 0);
        AddStrConf(OCK_MMC_LOG_LEVEL,
                   VNoCheck::Create());

        AddBoolConf(OCK_MMC_TLS_ENABLE, VNoCheck::Create());
        AddStrConf(OCK_MMC_TLS_TOP_PATH, VStrLength::Create(OCK_MMC_TLS_TOP_PATH.first, PATH_MAX_LEN));
        AddStrConf(OCK_MMC_TLS_CA_PATH, VStrLength::Create(OCK_MMC_TLS_CA_PATH.first, PATH_MAX_LEN));
        AddStrConf(OCK_MMC_TLS_CRL_PATH, VStrLength::Create(OCK_MMC_TLS_CRL_PATH.first, PATH_MAX_LEN));
        AddStrConf(OCK_MMC_TLS_CERT_PATH, VStrLength::Create(OCK_MMC_TLS_CERT_PATH.first, PATH_MAX_LEN));
        AddStrConf(OCK_MMC_TLS_KEY_PATH, VStrLength::Create(OCK_MMC_TLS_KEY_PATH.first, PATH_MAX_LEN));
        AddStrConf(OCK_MMC_TLS_PACKAGE_PATH, VStrLength::Create(OCK_MMC_TLS_PACKAGE_PATH.first, PATH_MAX_LEN));
    }

    void GetMetaServiceConfig(mmc_meta_service_config_t &config) {
        const auto discoveryURL = GetString(ConfConstant::OCK_MMC_META_SERVICE_URL);
        strncpy(config.discoveryURL, discoveryURL.c_str(), DISCOVERY_URL_SIZE);
        std::string logLevelStr = GetString(ConfConstant::OCK_MMC_LOG_LEVEL);
        config.logLevel = ock::mmc::MmcOutLogger::Instance().GetLogLevel(logLevelStr);
        GetTlsConfig(config.tlsConfig);
    }
};

class ClientConfig final: public Configuration {
public:
    void LoadDefault() override {
        using namespace ConfConstant;
        AddStrConf(OCK_MMC_META_SERVICE_URL, VNoCheck::Create(), 0);
        AddStrConf(OCK_MMC_LOG_LEVEL,
                   VNoCheck::Create());

        AddBoolConf(OCK_MMC_TLS_ENABLE, VNoCheck::Create());
        AddStrConf(OCK_MMC_TLS_TOP_PATH, VStrLength::Create(OCK_MMC_TLS_TOP_PATH.first, PATH_MAX_LEN));
        AddStrConf(OCK_MMC_TLS_CA_PATH, VStrLength::Create(OCK_MMC_TLS_CA_PATH.first, PATH_MAX_LEN));
        AddStrConf(OCK_MMC_TLS_CRL_PATH, VStrLength::Create(OCK_MMC_TLS_CRL_PATH.first, PATH_MAX_LEN));
        AddStrConf(OCK_MMC_TLS_CERT_PATH, VStrLength::Create(OCK_MMC_TLS_CERT_PATH.first, PATH_MAX_LEN));
        AddStrConf(OCK_MMC_TLS_KEY_PATH, VStrLength::Create(OCK_MMC_TLS_KEY_PATH.first, PATH_MAX_LEN));
        AddStrConf(OCK_MMC_TLS_PACKAGE_PATH, VStrLength::Create(OCK_MMC_TLS_PACKAGE_PATH.first, PATH_MAX_LEN));

        AddIntConf(OKC_MMC_LOCAL_SERVICE_DEVICE_ID,
            VIntRange::Create(OKC_MMC_LOCAL_SERVICE_DEVICE_ID.first, MIN_DEVICE_ID, MAX_DEVICE_ID));
        AddIntConf(OKC_MMC_LOCAL_SERVICE_RANK_ID,
            VIntRange::Create(OKC_MMC_LOCAL_SERVICE_RANK_ID.first, MIN_BM_RANK_ID, MAX_BM_RANK_ID));
        AddIntConf(OKC_MMC_LOCAL_SERVICE_WORLD_SIZE,
            VIntRange::Create(OKC_MMC_LOCAL_SERVICE_WORLD_SIZE.first, MIN_WORLD_SIZE, MAX_WORLD_SIZE));
        AddStrConf(OKC_MMC_LOCAL_SERVICE_BM_IP_PORT, VNoCheck::Create());
        AddStrConf(OKC_MMC_LOCAL_SERVICE_BM_HCOM_URL, VNoCheck::Create());
        AddIntConf(OKC_MMC_LOCAL_SERVICE_AUTO_RANKING,
            VIntRange::Create(OKC_MMC_LOCAL_SERVICE_AUTO_RANKING.first, 0, 1));
        AddStrConf(OKC_MMC_LOCAL_SERVICE_PROTOCOL, VNoCheck::Create());
        AddUInt64Conf(OKC_MMC_LOCAL_SERVICE_DRAM_SIZE,
            VUInt64Range::Create(OKC_MMC_LOCAL_SERVICE_DRAM_SIZE.first, 0, MAX_DRAM_SIZE));
        AddUInt64Conf(OKC_MMC_LOCAL_SERVICE_HBM_SIZE,
            VUInt64Range::Create(OKC_MMC_LOCAL_SERVICE_HBM_SIZE.first, 0, MAX_HBM_SIZE));
        AddIntConf(OCK_MMC_CLIENT_TIMEOUT_SECONDS,
            VIntRange::Create(OCK_MMC_CLIENT_TIMEOUT_SECONDS.first, 1, 600));
    }

    void GetLocalServiceConfig(mmc_local_service_config_t &config) {
        const auto discoveryURL = GetString(ConfConstant::OCK_MMC_META_SERVICE_URL);
        strncpy(config.discoveryURL, discoveryURL.c_str(), DISCOVERY_URL_SIZE);
        config.deviceId = GetInt(ConfConstant::OKC_MMC_LOCAL_SERVICE_DEVICE_ID);
        config.rankId = GetInt(ConfConstant::OKC_MMC_LOCAL_SERVICE_RANK_ID);
        config.worldSize = GetInt(ConfConstant::OKC_MMC_LOCAL_SERVICE_WORLD_SIZE);
        config.bmIpPort = GetString(ConfConstant::OKC_MMC_LOCAL_SERVICE_BM_IP_PORT);
        config.bmHcomUrl = GetString(ConfConstant::OKC_MMC_LOCAL_SERVICE_BM_HCOM_URL);
        config.autoRanking = GetInt(ConfConstant::OKC_MMC_LOCAL_SERVICE_AUTO_RANKING);
        config.createId = 0;
        config.dataOpType = GetString(ConfConstant::OKC_MMC_LOCAL_SERVICE_PROTOCOL);
        config.localDRAMSize = GetUInt64(ConfConstant::OKC_MMC_LOCAL_SERVICE_DRAM_SIZE);
        config.localHBMSize = GetUInt64(ConfConstant::OKC_MMC_LOCAL_SERVICE_HBM_SIZE);
        const auto logLevelStr = GetString(ConfConstant::OCK_MMC_LOG_LEVEL);
        config.logLevel = ock::mmc::MmcOutLogger::Instance().GetLogLevel(logLevelStr);
        GetTlsConfig(config.tlsConfig);
    }

    void GetClientConfig(mmc_client_config_t &config) {
        const auto discoveryURL = GetString(ConfConstant::OCK_MMC_META_SERVICE_URL);
        strncpy(config.discoveryURL, discoveryURL.c_str(), DISCOVERY_URL_SIZE);
        config.rankId = GetInt(ConfConstant::OKC_MMC_LOCAL_SERVICE_RANK_ID);
        config.timeOut = GetInt(ConfConstant::OCK_MMC_CLIENT_TIMEOUT_SECONDS);
        config.autoRanking = GetInt(ConfConstant::OKC_MMC_LOCAL_SERVICE_AUTO_RANKING);
        const auto logLevelStr = GetString(ConfConstant::OCK_MMC_LOG_LEVEL);
        config.logLevel = ock::mmc::MmcOutLogger::Instance().GetLogLevel(logLevelStr);
        GetTlsConfig(config.tlsConfig);
    }

    static Result ValidateLocalServiceConfig(const mmc_local_service_config_t &config)
    {
        if (config.localDRAMSize == 0 && config.localHBMSize == 0) {
            MMC_LOG_ERROR("DRAM size and HBM size cannot be zero at the same time");
            return MMC_INVALID_PARAM;
        }
        if (config.localDRAMSize > 0 && config.localHBMSize > 0) {
            MMC_LOG_ERROR("DRAM pool and HBM pool cannot be used simultaneously");
            return MMC_INVALID_PARAM;
        }
        if (config.localDRAMSize % DRAM_SIZE_ALIGNMENT != 0) {
            MMC_LOG_ERROR("DRAM size should be aligned at a integer multiple of 2M (2097152 bytes)");
            return MMC_INVALID_PARAM;
        }
        return MMC_OK;
    }
};

} // namespace mmc
} // namespace ock