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

namespace ock {
namespace mmc {
constexpr uint32_t CONF_MUST = 1;

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
    std::vector<std::string> Validate(bool isAuth = false, bool isTLS = false, bool isAuthor = false,
        bool isZKSecure = false);
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
        AddIntConf(OCK_MMC_META_SERVICE_LOG_LEVEL,
                   VIntRange::Create(OCK_MMC_META_SERVICE_LOG_LEVEL.first, DEBUG_LEVEL, BUTT_LEVEL));

        AddBoolConf(OCK_MMC_TLS_ENABLE, VNoCheck::Create());
        AddStrConf(OCK_MMC_TLS_TOP_PATH, VNoCheck::Create());
        AddStrConf(OCK_MMC_TLS_CA_PATH, VNoCheck::Create());
        AddStrConf(OCK_MMC_TLS_CRL_PATH, VNoCheck::Create());
        AddStrConf(OCK_MMC_TLS_CERT_PATH, VNoCheck::Create());
        AddStrConf(OCK_MMC_TLS_KEY_PATH, VNoCheck::Create());
        AddStrConf(OCK_MMC_TLS_PACKAGE_PATH, VNoCheck::Create());
    }

    void GetMetaServiceConfig(mmc_meta_service_config_t &config) {
        const auto discoveryURL = GetString(ConfConstant::OCK_MMC_META_SERVICE_URL);
        strncpy(config.discoveryURL, discoveryURL.c_str(), DISCOVERY_URL_SIZE);
        config.logLevel = GetInt(ConfConstant::OCK_MMC_META_SERVICE_LOG_LEVEL);
        GetTlsConfig(config.tlsConfig);
    }
};

class ClientConfig final: public Configuration {
public:
    void LoadDefault() override {
        using namespace ConfConstant;
        AddStrConf(OCK_MMC_META_SERVICE_URL, VNoCheck::Create(), 0);

        AddBoolConf(OCK_MMC_TLS_ENABLE, VNoCheck::Create());
        AddStrConf(OCK_MMC_TLS_TOP_PATH, VNoCheck::Create());
        AddStrConf(OCK_MMC_TLS_CA_PATH, VNoCheck::Create());
        AddStrConf(OCK_MMC_TLS_CRL_PATH, VNoCheck::Create());
        AddStrConf(OCK_MMC_TLS_CERT_PATH, VNoCheck::Create());
        AddStrConf(OCK_MMC_TLS_KEY_PATH, VNoCheck::Create());
        AddStrConf(OCK_MMC_TLS_PACKAGE_PATH, VNoCheck::Create());

        AddIntConf(OKC_MMC_LOCAL_SERVICE_DEVICE_ID, VNoCheck::Create());
        AddIntConf(OKC_MMC_LOCAL_SERVICE_RANK_ID, VNoCheck::Create());
        AddIntConf(OKC_MMC_LOCAL_SERVICE_WORLD_SIZE, VNoCheck::Create());
        AddStrConf(OKC_MMC_LOCAL_SERVICE_BM_IP_PORT, VNoCheck::Create());
        AddStrConf(OKC_MMC_LOCAL_SERVICE_BM_HCOM_URL, VNoCheck::Create());
        AddIntConf(OKC_MMC_LOCAL_SERVICE_AUTO_RANKING, VNoCheck::Create());
        AddIntConf(OKC_MMC_LOCAL_SERVICE_BM_ID, VNoCheck::Create());
        AddStrConf(OKC_MMC_LOCAL_SERVICE_PROTOCOL, VNoCheck::Create());
        AddUInt64Conf(OKC_MMC_LOCAL_SERVICE_DRAM_SIZE, VNoCheck::Create());
        AddUInt64Conf(OKC_MMC_LOCAL_SERVICE_HBM_SIZE, VNoCheck::Create());

        AddIntConf(OCK_MMC_CLIENT_TIMEOUT_SECONDS, VNoCheck::Create());
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
        config.createId = GetInt(ConfConstant::OKC_MMC_LOCAL_SERVICE_BM_ID);
        config.dataOpType = GetString(ConfConstant::OKC_MMC_LOCAL_SERVICE_PROTOCOL);
        config.localDRAMSize = GetUInt64(ConfConstant::OKC_MMC_LOCAL_SERVICE_DRAM_SIZE);
        config.localHBMSize = GetUInt64(ConfConstant::OKC_MMC_LOCAL_SERVICE_HBM_SIZE);
        config.logLevel = 0;
        config.logFunc = nullptr;
        GetTlsConfig(config.tlsConfig);
    }

    void GetClientConfig(mmc_client_config_t &config) {
        const auto discoveryURL = GetString(ConfConstant::OCK_MMC_META_SERVICE_URL);
        strncpy(config.discoveryURL, discoveryURL.c_str(), DISCOVERY_URL_SIZE);
        config.rankId = GetInt(ConfConstant::OKC_MMC_LOCAL_SERVICE_RANK_ID);
        config.timeOut = GetInt(ConfConstant::OCK_MMC_CLIENT_TIMEOUT_SECONDS);
        config.autoRanking = GetInt(ConfConstant::OKC_MMC_LOCAL_SERVICE_AUTO_RANKING);
        GetTlsConfig(config.tlsConfig);
    }
};

} // namespace mmc
} // namespace ock