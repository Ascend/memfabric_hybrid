/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */

#pragma once

#include <map>
#include <utility>
#include <vector>
#include <cctype>

#include "mmc_lock.h"
#include "mmc_ref.h"
#include "mmc_config_validator.h"
#include "mmc_config_convertor.h"
#include "mmc_config_const.h"
#include "mmc_def.h"
#include "mmc_logger.h"
#include "mmc_types.h"
#include "smem_bm_def.h"

namespace ock {
namespace mmc {
constexpr uint32_t CONF_MUST = 1;
constexpr uint64_t DRAM_SIZE_ALIGNMENT = 2097152; // 2MB
constexpr uint64_t HBM_SIZE_ALIGNMENT = 2097152; // 2MB

// 定义单位与字节的转换关系
enum class MemUnit {
    B,
    KB,
    MB,
    GB,
    TB,
    UNKNOWN
};

enum class ConfValueType {
    VINT = 0,
    VFLOAT = 1,
    VSTRING = 2,
    VBOOL = 3,
    VUINT64 = 4,
};

static void StringToLower(std::string &str)
{
    for (char &c : str) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
}

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
    uint64_t GetUInt64(const char *key, uint64_t defaultValue);
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
    void GetTlsConfig(mf::tls_config &tlsConfig);

    static int ValidateTLSConfig(const mf::tls_config &tlsConfig);

    const std::string GetBinDir();
    const std::string GetLogPath(const std::string &logPath);
    static int ValidateLogPathConfig(const std::string &logPath);

    bool Initialized() const
    {
        return mInitialized;
    }

private:
    bool SetWithStrAutoConvert(const std::string &key, const std::string &value);
    uint64_t ParseMemSize(const std::string &memStr);
    MemUnit ParseMemUnit(const std::string& unit);

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
        AddStrConf(OCK_MMC_META_SERVICE_CONFIG_STORE_URL, VNoCheck::Create(), 0);
        AddBoolConf(OCK_MMC_META_HA_ENABLE, VNoCheck::Create());
        AddStrConf(OCK_MMC_LOG_LEVEL, VNoCheck::Create());
        AddStrConf(OCK_MMC_LOG_PATH, VStrLength::Create(OCK_MMC_LOG_PATH.first, PATH_MAX_LEN));
        AddIntConf(OCK_MMC_LOG_ROTATION_FILE_SIZE, VIntRange::Create(OCK_MMC_LOG_ROTATION_FILE_SIZE.first,
            MIN_LOG_ROTATION_FILE_SIZE, MAX_LOG_ROTATION_FILE_SIZE));
        AddIntConf(OCK_MMC_LOG_ROTATION_FILE_COUNT, VIntRange::Create(OCK_MMC_LOG_ROTATION_FILE_COUNT.first,
            MIN_LOG_ROTATION_FILE_COUNT, MAX_LOG_ROTATION_FILE_COUNT));
        AddIntConf(OKC_MMC_EVICT_THRESHOLD_HIGH,
            VIntRange::Create(OKC_MMC_EVICT_THRESHOLD_HIGH.first, MIN_EVICT_THRESHOLD, MAX_EVICT_THRESHOLD));
        AddIntConf(OKC_MMC_EVICT_THRESHOLD_LOW,
            VIntRange::Create(OKC_MMC_EVICT_THRESHOLD_LOW.first, MIN_EVICT_THRESHOLD, MAX_EVICT_THRESHOLD - 1));

        AddBoolConf(OCK_MMC_TLS_ENABLE, VNoCheck::Create());
        AddStrConf(OCK_MMC_TLS_CA_PATH, VStrLength::Create(OCK_MMC_TLS_CA_PATH.first, PATH_MAX_LEN));
        AddStrConf(OCK_MMC_TLS_CRL_PATH, VStrLength::Create(OCK_MMC_TLS_CRL_PATH.first, PATH_MAX_LEN));
        AddStrConf(OCK_MMC_TLS_CERT_PATH, VStrLength::Create(OCK_MMC_TLS_CERT_PATH.first, PATH_MAX_LEN));
        AddStrConf(OCK_MMC_TLS_KEY_PATH, VStrLength::Create(OCK_MMC_TLS_KEY_PATH.first, PATH_MAX_LEN));
        AddStrConf(OCK_MMC_TLS_KEY_PASS_PATH, VStrLength::Create(OCK_MMC_TLS_KEY_PASS_PATH.first, PATH_MAX_LEN));
        AddStrConf(OCK_MMC_TLS_PACKAGE_PATH, VStrLength::Create(OCK_MMC_TLS_PACKAGE_PATH.first, PATH_MAX_LEN));
        AddStrConf(OCK_MMC_TLS_DECRYPTER_PATH, VStrLength::Create(OCK_MMC_TLS_DECRYPTER_PATH.first, PATH_MAX_LEN));
    }

    void GetMetaServiceConfig(mmc_meta_service_config_t &config) {
        const auto discoveryURL = GetString(ConfConstant::OCK_MMC_META_SERVICE_URL);
        size_t copy_count = std::min(std::strlen(discoveryURL.c_str()), static_cast<size_t>(DISCOVERY_URL_SIZE - 1));
        std::copy_n(discoveryURL.c_str(), copy_count, config.discoveryURL);
        config.discoveryURL[copy_count] = '\0';

        const auto configStoreURL = GetString(ConfConstant::OCK_MMC_META_SERVICE_CONFIG_STORE_URL);
        copy_count = std::min(std::strlen(configStoreURL.c_str()), static_cast<size_t>(DISCOVERY_URL_SIZE - 1));
        std::copy_n(configStoreURL.c_str(), copy_count, config.configStoreURL);
        config.configStoreURL[copy_count] = '\0';

        config.haEnable = GetBool(ConfConstant::OCK_MMC_META_HA_ENABLE);
        std::string logLevelStr = GetString(ConfConstant::OCK_MMC_LOG_LEVEL);
        StringToLower(logLevelStr);
        config.logLevel = ock::mmc::MmcOutLogger::Instance().GetLogLevel(logLevelStr);

        const auto logPath = GetLogPath(GetString(ConfConstant::OCK_MMC_LOG_PATH));
        copy_count = std::min(std::strlen(logPath.c_str()), static_cast<size_t>(PATH_MAX_SIZE - 1));
        std::copy_n(logPath.c_str(), copy_count, config.logPath);
        config.logPath[copy_count] = '\0';

        config.evictThresholdHigh = GetInt(ConfConstant::OKC_MMC_EVICT_THRESHOLD_HIGH);
        config.evictThresholdLow = GetInt(ConfConstant::OKC_MMC_EVICT_THRESHOLD_LOW);
        config.logRotationFileSize = GetInt(ConfConstant::OCK_MMC_LOG_ROTATION_FILE_SIZE) * MB_NUM;
        config.logRotationFileCount = GetInt(ConfConstant::OCK_MMC_LOG_ROTATION_FILE_COUNT);
        GetTlsConfig(config.tlsConfig);
    }
};

class ClientConfig final: public Configuration {
public:
    void LoadDefault() override {
        using namespace ConfConstant;
        AddStrConf(OCK_MMC_META_SERVICE_URL, VNoCheck::Create(), 0);
        AddStrConf(OCK_MMC_LOG_LEVEL, VNoCheck::Create());

        AddBoolConf(OCK_MMC_TLS_ENABLE, VNoCheck::Create());
        AddStrConf(OCK_MMC_TLS_CA_PATH, VStrLength::Create(OCK_MMC_TLS_CA_PATH.first, PATH_MAX_LEN));
        AddStrConf(OCK_MMC_TLS_CRL_PATH, VStrLength::Create(OCK_MMC_TLS_CRL_PATH.first, PATH_MAX_LEN));
        AddStrConf(OCK_MMC_TLS_CERT_PATH, VStrLength::Create(OCK_MMC_TLS_CERT_PATH.first, PATH_MAX_LEN));
        AddStrConf(OCK_MMC_TLS_KEY_PATH, VStrLength::Create(OCK_MMC_TLS_KEY_PATH.first, PATH_MAX_LEN));
        AddStrConf(OCK_MMC_TLS_KEY_PASS_PATH, VStrLength::Create(OCK_MMC_TLS_KEY_PASS_PATH.first, PATH_MAX_LEN));
        AddStrConf(OCK_MMC_TLS_PACKAGE_PATH, VStrLength::Create(OCK_MMC_TLS_PACKAGE_PATH.first, PATH_MAX_LEN));
        AddStrConf(OCK_MMC_TLS_DECRYPTER_PATH, VStrLength::Create(OCK_MMC_TLS_DECRYPTER_PATH.first, PATH_MAX_LEN));

        AddIntConf(OKC_MMC_LOCAL_SERVICE_WORLD_SIZE,
            VIntRange::Create(OKC_MMC_LOCAL_SERVICE_WORLD_SIZE.first, MIN_WORLD_SIZE, MAX_WORLD_SIZE));
        AddStrConf(OKC_MMC_LOCAL_SERVICE_BM_IP_PORT, VNoCheck::Create());
        AddStrConf(OKC_MMC_LOCAL_SERVICE_BM_HCOM_URL, VNoCheck::Create());
        AddStrConf(OKC_MMC_LOCAL_SERVICE_PROTOCOL, VNoCheck::Create());
        AddStrConf(OKC_MMC_LOCAL_SERVICE_DRAM_SIZE, VNoCheck::Create());
        AddStrConf(OKC_MMC_LOCAL_SERVICE_HBM_SIZE, VNoCheck::Create());
        AddIntConf(OKC_MMC_CLIENT_RETRY_MILLISECONDS,
                   VIntRange::Create(OKC_MMC_CLIENT_RETRY_MILLISECONDS.first, 0, 600000));
        AddBoolConf(OKC_MMC_LOCAL_SERVICE_DRAM_BY_SDMA, VNoCheck::Create());
        AddIntConf(OCK_MMC_CLIENT_TIMEOUT_SECONDS,
            VIntRange::Create(OCK_MMC_CLIENT_TIMEOUT_SECONDS.first, 1, 600));
    }

    void GetLocalServiceConfig(mmc_local_service_config_t &config) {
        const auto discoveryURL = GetString(ConfConstant::OCK_MMC_META_SERVICE_URL);
        size_t copy_count = std::min(std::strlen(discoveryURL.c_str()), static_cast<size_t>(DISCOVERY_URL_SIZE - 1));
        std::copy_n(discoveryURL.c_str(), copy_count, config.discoveryURL);
        config.discoveryURL[copy_count] = '\0';

        config.worldSize = GetInt(ConfConstant::OKC_MMC_LOCAL_SERVICE_WORLD_SIZE);
        config.bmIpPort = GetString(ConfConstant::OKC_MMC_LOCAL_SERVICE_BM_IP_PORT);
        config.bmHcomUrl = GetString(ConfConstant::OKC_MMC_LOCAL_SERVICE_BM_HCOM_URL);
        config.createId = 0;
        config.dataOpType = GetString(ConfConstant::OKC_MMC_LOCAL_SERVICE_PROTOCOL);
        config.localDRAMSize = GetUInt64(ConfConstant::OKC_MMC_LOCAL_SERVICE_DRAM_SIZE.first, MEM_128MB_BYTES);
        config.localHBMSize = GetUInt64(ConfConstant::OKC_MMC_LOCAL_SERVICE_HBM_SIZE.first, MEM_2MB_BYTES);
        if (GetBool(ConfConstant::OKC_MMC_LOCAL_SERVICE_DRAM_BY_SDMA)) {
            config.flags |= SMEM_BM_INIT_GVM_FLAG;
        }
        std::string logLevelStr = GetString(ConfConstant::OCK_MMC_LOG_LEVEL);
        StringToLower(logLevelStr);
        config.logLevel = MmcOutLogger::Instance().GetLogLevel(logLevelStr);
        GetTlsConfig(config.tlsConfig);
    }

    void GetClientConfig(mmc_client_config_t &config) {
        const auto discoveryURL = GetString(ConfConstant::OCK_MMC_META_SERVICE_URL);
        size_t copy_count = std::min(std::strlen(discoveryURL.c_str()), static_cast<size_t>(DISCOVERY_URL_SIZE - 1));
        std::copy_n(discoveryURL.c_str(), copy_count, config.discoveryURL);
        config.discoveryURL[copy_count] = '\0';

        config.rpcRetryTimeOut = GetInt(ConfConstant::OKC_MMC_CLIENT_RETRY_MILLISECONDS);
        config.timeOut = GetInt(ConfConstant::OCK_MMC_CLIENT_TIMEOUT_SECONDS);
        std::string logLevelStr = GetString(ConfConstant::OCK_MMC_LOG_LEVEL);
        StringToLower(logLevelStr);
        config.logLevel = MmcOutLogger::Instance().GetLogLevel(logLevelStr);
        GetTlsConfig(config.tlsConfig);
    }

    static Result ValidateLocalServiceConfig(mmc_local_service_config_t &config)
    {
        if (config.localDRAMSize > MAX_DRAM_SIZE) {
            MMC_LOG_ERROR("After alignment 2MB, DRAM size (" << config.localDRAMSize << ") exceeds 1TB");
            return MMC_INVALID_PARAM;
        }

        if (config.localHBMSize > MAX_HBM_SIZE) {
            MMC_LOG_ERROR("After alignment 2MB, HBM size (" << config.localHBMSize << ") exceeds 1TB");
            return MMC_INVALID_PARAM;
        }

        config.localDRAMSize = (config.localDRAMSize / DRAM_SIZE_ALIGNMENT) * DRAM_SIZE_ALIGNMENT;
        config.localHBMSize = (config.localHBMSize / DRAM_SIZE_ALIGNMENT) * DRAM_SIZE_ALIGNMENT;

        MMC_LOG_INFO("After alignment 2MB, DRAM size is " << config.localDRAMSize);
        MMC_LOG_INFO("After alignment 2MB, HBM size is " << config.localHBMSize);

        if (config.localDRAMSize == 0 && config.localHBMSize == 0) {
            MMC_LOG_ERROR("After alignment 2MB, DRAM size and HBM size cannot be 0 at the same time");
            return MMC_INVALID_PARAM;
        }

        return MMC_OK;
    }
};

} // namespace mmc
} // namespace ock