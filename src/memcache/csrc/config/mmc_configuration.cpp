/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */

#include "mmc_configuration.h"

#include <iostream>
#include <mmc_functions.h>

#include "mmc_kv_parser.h"

namespace ock {
namespace mmc {

static constexpr int MAX_CONF_ITEM_COUNT = 100;

void StringToLower(std::string &str)
{
    for (char &c : str) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
}

Configuration::~Configuration()
{
    for (auto validator : mValueValidator) {
        validator.second.Set(nullptr);
    }
    for (auto converter : mValueConverter) {
        converter.second.Set(nullptr);
    }
    mValueValidator.clear();
    mValueConverter.clear();
}

bool Configuration::LoadFromFile(const std::string &filePath)
{
    LoadConfigurations();
    if (!Initialized()) {
        return false;
    }
    auto *kvParser = new (std::nothrow) KVParser();
    if (kvParser == nullptr) {
        return false;
    }
    if (kvParser->FromFile(filePath) != MMC_OK) {
        SAFE_DELETE(kvParser);
        return false;
    }

    uint32_t size = kvParser->Size();
    if (size > MAX_CONF_ITEM_COUNT) {
        SAFE_DELETE(kvParser);
        return false;
    }
    for (uint32_t i = 0; i < size; i++) {
        std::string key;
        std::string value;
        kvParser->GetI(i, key, value);
        if (!SetWithTypeAutoConvert(key, value)) {
            SAFE_DELETE(kvParser);
            return false;
        }
    }

    if (!kvParser->CheckSet(mMustKeys)) {
        SAFE_DELETE(kvParser);
        return false;
    }
    SAFE_DELETE(kvParser);
    return true;
}


int32_t Configuration::GetInt(const std::pair<const char *, int32_t> &item)
{
    GUARD(&mLock, mLock);
    const auto iter = mIntItems.find(item.first);
    if (iter != mIntItems.end()) {
        return iter->second;
    }
    return item.second;
}

float Configuration::GetFloat(const std::pair<const char *, float> &item)
{
    GUARD(&mLock, mLock);
    const auto iter = mFloatItems.find(item.first);
    if (iter != mFloatItems.end()) {
        return iter->second;
    }
    return item.second;
}

std::string Configuration::GetString(const std::pair<const char *, const char *> &item)
{
    GUARD(&mLock, mLock);
    const auto iter = mStrItems.find(item.first);
    if (iter != mStrItems.end()) {
        return iter->second;
    }
    return item.second;
}

bool Configuration::GetBool(const std::pair<const char *, bool> &item)
{
    GUARD(&mLock, mLock);
    const auto iter = mBoolItems.find(item.first);
    if (iter != mBoolItems.end()) {
        return iter->second;
    }
    return item.second;
}

uint64_t Configuration::GetUInt64(const std::pair<const char *, uint64_t> &item)
{
    GUARD(&mLock, mLock);
    const auto iter = mUInt64Items.find(item.first);
    if (iter != mUInt64Items.end()) {
        return iter->second;
    }
    return item.second;
}

uint64_t Configuration::GetUInt64(const char * key, uint64_t defaultValue)
{
    GUARD(&mLock, mLock);
    const auto iter = mUInt64Items.find(key);
    if (iter != mUInt64Items.end()) {
        return iter->second;
    }
    return defaultValue;
}

std::string Configuration::GetConvertedValue(const std::string &key)
{
    GUARD(&mLock, mLock);
    auto iter = mValueTypes.find(key);
    if (iter == mValueTypes.end()) {
        return "";
    }
    ConfValueType valueType = iter->second;
    std::string value = std::string();
    switch (valueType) {
        case ConfValueType::VSTRING: {
            auto iterStr = mStrItems.find(key);
            value = iterStr != mStrItems.end() ? iterStr->second : std::string();
            break;
        }
        case ConfValueType::VBOOL: {
            auto iterBool = mBoolItems.find(key);
            value = iterBool != mBoolItems.end() ? std::to_string(iterBool->second) : std::string();
            break;
        }
        case ConfValueType::VUINT64: {
            auto iterUInt64 = mUInt64Items.find(key);
            value = iterUInt64 != mUInt64Items.end() ? std::to_string(iterUInt64->second) : std::string();
            break;
        }
        case ConfValueType::VINT: {
            auto iterInt = mIntItems.find(key);
            value = iterInt != mIntItems.end() ? std::to_string(iterInt->second) : std::string();
            break;
        }
        case ConfValueType::VFLOAT: {
            auto iterFloat = mFloatItems.find(key);
            value = iterFloat != mFloatItems.end() ? std::to_string(iterFloat->second) : std::string();
            break;
        }
        default:;
    }
    auto converterIter = mValueConverter.find(key);
    if (converterIter == mValueConverter.end() || converterIter->second == nullptr) {
        return value;
    }
    return converterIter->second->Convert(value);
}

void Configuration::Set(const std::string &key, int32_t value)
{
    GUARD(&mLock, mLock);
    if (mIntItems.count(key) > 0) {
        mIntItems.at(key) = value;
    }
}

void Configuration::Set(const std::string &key, float value)
{
    GUARD(&mLock, mLock);
    if (mFloatItems.count(key) > 0) {
        mFloatItems.at(key) = value;
    }
}

void Configuration::Set(const std::string &key, const std::string &value)
{
    GUARD(&mLock, mLock);
    if (mStrItems.count(key) > 0) {
        mStrItems.at(key) = value;
    }
}

void Configuration::Set(const std::string &key, bool value)
{
    GUARD(&mLock, mLock);
    if (mBoolItems.count(key) > 0) {
        mBoolItems.at(key) = value;
    }
}

void Configuration::Set(const std::string &key, uint64_t value)
{
    GUARD(&mLock, mLock);
    if (mUInt64Items.count(key) > 0) {
        mUInt64Items.at(key) = value;
    }
}

bool Configuration::SetWithTypeAutoConvert(const std::string &key, const std::string &value)
{
    GUARD(&mLock, mLock);
    auto iter = mValueTypes.find(key);
    auto iterIgnored = std::find(mInvalidSetConfs.begin(), mInvalidSetConfs.end(), key);
    if (iter == mValueTypes.end() || iterIgnored != mInvalidSetConfs.end()) {
        std::cerr << "Invalid key <" << key << ">." << std::endl;
        return false;
    }
    ConfValueType valueType = iter->second;
    if (valueType == ConfValueType::VINT) {
        long tmp = 0;
        if (!OckStol(value, tmp) || tmp > INT32_MAX || tmp < INT32_MIN) {
            std::cerr << "<" << key << "> was empty or in wrong type, it should be a int number." << std::endl;
            return false;
        }
        mIntItems[key] = static_cast<int32_t>(tmp);
    } else if (valueType == ConfValueType::VFLOAT) {
        float tmp = 0.0;
        if (!OckStof(value, tmp)) {
            std::cerr << "<" << key << "> was empty or in wrong type, it should be a float number." << std::endl;
            return false;
        }
        mFloatItems[key] = tmp;
    } else if (valueType == ConfValueType::VSTRING) {
        if (mStrItems.count(key) > 0) {
            return SetWithStrAutoConvert(key, value);
        }
    } else if (valueType == ConfValueType::VBOOL) {
        bool b = false;
        if (!IsBool(value, b)) {
            std::cerr << "<" << key << "> should represent a bool value." << std::endl;
            return false;
        }
        mBoolItems[key] = b;
    } else if (valueType == ConfValueType::VUINT64) {
        uint64_t tmp = 0;
        if (!OckStoULL(value, tmp)) {
            std::cerr << "<" << key << "> was empty or in wrong type, it should be a unsigned long long number."
                << std::endl;
            return false;
        }
        mUInt64Items[key] = tmp;
    }
    return true;
}

bool Configuration::SetWithStrAutoConvert(const std::string &key, const std::string &value)
{
    std::string tempValue = value;
    if (key == ConfConstant::OKC_MMC_LOCAL_SERVICE_DRAM_SIZE.first
        || key == ConfConstant::OKC_MMC_LOCAL_SERVICE_HBM_SIZE.first) {
        auto memSize = ParseMemSize(tempValue);
        if (memSize == UINT64_MAX) {
            std::cerr << "DRAM or HBM value (" << tempValue <<") is invalid, "
                      << "please check 'ock.mmc.local_service.dram.size' "
                      << "or 'ock.mmc.local_service.hbm.size'" << std::endl;
            return false;
        }
        mUInt64Items.insert(std::make_pair(key, memSize));
    }
    if (find(mPathConfs.begin(), mPathConfs.end(), key) != mPathConfs.end() &&
        !GetRealPath(tempValue)) { // 简化路径为绝对路径
        std::cerr << "Simplify <" << key << "> to absolute path failed." << std::endl;
        return false;
    }
    mStrItems[key] = tempValue;
    return true;
}

void Configuration::SetValidator(const std::string &key, const ValidatorPtr &validator, uint32_t flag)
{
    if (validator == nullptr) {
        std::string errorInfo = "The validator of <" + key + "> create failed, maybe out of memory.";
        mLoadDefaultErrors.push_back(errorInfo);
        return;
    }
    if (mValueValidator.find(key) == mValueValidator.end()) {
        mValueValidator.insert(std::make_pair(key, validator));
    } else {
        mValueValidator.at(key) = validator;
    }
    if (flag & CONF_MUST) {
        mMustKeys.push_back(key);
    }
}

void Configuration::AddIntConf(const std::pair<std::string, int> &pair, const ValidatorPtr &validator, uint32_t flag)
{
    mIntItems.insert(pair);
    mValueTypes.insert(std::make_pair(pair.first, ConfValueType::VINT));
    SetValidator(pair.first, validator, flag);
}

void Configuration::AddStrConf(const std::pair<std::string, std::string> &pair, const ValidatorPtr &validator,
    uint32_t flag)
{
    mStrItems.insert(pair);
    mValueTypes.insert(std::make_pair(pair.first, ConfValueType::VSTRING));
    SetValidator(pair.first, validator, flag);
}

void Configuration::AddBoolConf(const std::pair<std::string, bool> &pair, const ValidatorPtr &validator, uint32_t flag)
{
    mBoolItems.insert(pair);
    mValueTypes.insert(std::make_pair(pair.first, ConfValueType::VBOOL));
    SetValidator(pair.first, validator, flag);
}

void Configuration::AddUInt64Conf(const std::pair<std::string, uint64_t> &pair, const ValidatorPtr &validator,
    uint32_t flag)
{
    mUInt64Items.insert(pair);
    mValueTypes.insert(std::make_pair(pair.first, ConfValueType::VUINT64));
    SetValidator(pair.first, validator, flag);
}

void Configuration::AddPathConf(const std::pair<std::string, std::string> &pair, const ValidatorPtr &validator,
    uint32_t flag)
{
    AddStrConf(pair, validator, flag);
    mPathConfs.push_back(pair.first);
    SetValidator(pair.first, validator, flag);
}

void Configuration::AddConverter(const std::string &key, const ConverterPtr &converter)
{
    if (converter == nullptr) {
        std::string errorInfo = "The converter of <" + key + "> create failed, maybe out of memory.";
        mLoadDefaultErrors.push_back(errorInfo);
        return;
    }
    if (mValueConverter.find(key) == mValueConverter.end()) {
        mValueConverter.insert(std::make_pair(key, converter));
    } else {
        mValueConverter.at(key) = converter;
    }
}

void Configuration::ValidateOneType(const std::string &key, const ValidatorPtr &validator,
    std::vector<std::string> &errors, ConfValueType &vType)
{
    if (validator == nullptr) {
        errors.push_back("Failed to validate <" + key + ">, validator is NULL");
        return;
    }
    switch (vType) {
        case ConfValueType::VSTRING: {
            auto valueIterStr = mStrItems.find(key);
            if (valueIterStr == mStrItems.end()) {
                errors.push_back("Failed to find <" + key + "> in string value map, which should not happen.");
                break;
            }
            AddValidateError(validator, errors, valueIterStr);
            break;
        }
        case ConfValueType::VFLOAT: {
            auto valueIterFloat = mFloatItems.find(key);
            if (valueIterFloat == mFloatItems.end()) {
                errors.push_back("Failed to find <" + key + "> in float value map, which should not happen.");
                break;
            }
            AddValidateError(validator, errors, valueIterFloat);
            break;
        }
        case ConfValueType::VINT: {
            auto valueIterInt = mIntItems.find(key);
            if (valueIterInt == mIntItems.end()) {
                errors.push_back("Failed to find <" + key + "> in int value map, which should not happen.");
                break;
            }
            AddValidateError(validator, errors, valueIterInt);
            break;
        }
        case ConfValueType::VUINT64: {
            auto valueIterULL = mUInt64Items.find(key);
            if (valueIterULL == mUInt64Items.end()) {
                errors.push_back("Failed to find <" + key + "> in UInt64 value map, which should not happen.");
                break;
            }
            AddValidateError(validator, errors, valueIterULL);
            break;
        }
        default:;
    }
}

void Configuration::ValidateOneValueMap(std::vector<std::string> &errors,
    const std::map<std::string, ValidatorPtr> &valueValidator)
{
    for (auto &item : valueValidator) {
        auto valueValidatorPtr = item.second.Get();
        if (valueValidatorPtr == nullptr) {
            errors.push_back("The validator of <" + item.first + "> is null, skip.");
            continue;
        }
        // initialize, if failed then skip it
        if (!(valueValidatorPtr->Initialize())) {
            errors.push_back(valueValidatorPtr->ErrorMessage());
            continue;
        }

        // firstly find the value type
        auto typeIter = mValueTypes.find(item.first);
        if (typeIter == mValueTypes.end()) {
            errors.push_back("Failed to find <" + item.first + "> in type map, which should not happen.");
            continue;
        }

        ValidateOneType(item.first, item.second, errors, typeIter->second);
    }
}

void Configuration::ValidateItem(const std::string &itemKey, std::vector<std::string> &errors)
{
    auto validatorIter = mValueValidator.find(itemKey);
    if (validatorIter == mValueValidator.end()) {
        errors.push_back("Failed to find <" + itemKey + "> in Validator map, which should not happen.");
        return;
    }
    auto typeIter = mValueTypes.find(itemKey);
    if (typeIter == mValueTypes.end()) {
        errors.push_back("Failed to find <" + itemKey + "> in type map, which should not happen.");
        return;
    }
    ValidateOneType(validatorIter->first, validatorIter->second, errors, typeIter->second);
}

std::vector<std::string> Configuration::ValidateConf()
{
    using namespace ConfConstant;
    std::vector<std::string> errors;
    for (const auto& validate : mValueValidator) {
        if (validate.second == nullptr) {
            std::string errorInfo = "The validator of <" + validate.first + "> create failed, maybe out of memory.";
            errors.push_back(errorInfo);
            continue;
        }
        ValidateItem(validate.first, errors);
    }
    return errors;
}

void Configuration::LoadConfigurations()
{
    mLoadDefaultErrors.clear();
    mInitialized = false;
    LoadDefault();
    if (!mLoadDefaultErrors.empty()) {
        for (auto &errorInfo : mLoadDefaultErrors) {
            std::cerr << errorInfo << std::endl;
        }
        mLoadDefaultErrors.clear();
        return;
    }
    mLoadDefaultErrors.clear();
    mInitialized = true;
}

void Configuration::GetAccTlsConfig(tls_config &tlsConfig)
{
    tlsConfig.tlsEnable = GetBool(ConfConstant::OCK_MMC_TLS_ENABLE);
    std::copy_n(GetString(ConfConstant::OCK_MMC_TLS_CA_PATH).c_str(), TLS_PATH_MAX_LEN, tlsConfig.caPath);
    std::copy_n(GetString(ConfConstant::OCK_MMC_TLS_CRL_PATH).c_str(), TLS_PATH_MAX_LEN, tlsConfig.crlPath);
    std::copy_n(GetString(ConfConstant::OCK_MMC_TLS_CERT_PATH).c_str(), TLS_PATH_MAX_LEN, tlsConfig.certPath);
    std::copy_n(GetString(ConfConstant::OCK_MMC_TLS_KEY_PATH).c_str(), TLS_PATH_MAX_LEN, tlsConfig.keyPath);
    std::copy_n(GetString(ConfConstant::OCK_MMC_TLS_KEY_PASS_PATH).c_str(), TLS_PATH_MAX_LEN, tlsConfig.keyPassPath);
    std::copy_n(GetString(ConfConstant::OCK_MMC_TLS_PACKAGE_PATH).c_str(), TLS_PATH_MAX_LEN, tlsConfig.packagePath);
    std::copy_n(GetString(ConfConstant::OCK_MMC_TLS_DECRYPTER_PATH).c_str(),
        TLS_PATH_MAX_LEN, tlsConfig.decrypterLibPath);
}

void Configuration::GetHcomTlsConfig(tls_config &tlsConfig)
{
    tlsConfig.tlsEnable = GetBool(ConfConstant::OCK_MMC_HCOM_TLS_ENABLE);
    std::copy_n(GetString(ConfConstant::OCK_MMC_HCOM_TLS_CA_PATH).c_str(), TLS_PATH_MAX_LEN, tlsConfig.caPath);
    std::copy_n(GetString(ConfConstant::OCK_MMC_HCOM_TLS_CRL_PATH).c_str(), TLS_PATH_MAX_LEN, tlsConfig.crlPath);
    std::copy_n(GetString(ConfConstant::OCK_MMC_HCOM_TLS_CERT_PATH).c_str(), TLS_PATH_MAX_LEN, tlsConfig.certPath);
    std::copy_n(GetString(ConfConstant::OCK_MMC_HCOM_TLS_KEY_PATH).c_str(), TLS_PATH_MAX_LEN, tlsConfig.keyPath);
    std::copy_n(GetString(ConfConstant::OCK_MMC_HCOM_TLS_KEY_PASS_PATH).c_str(),
        TLS_PATH_MAX_LEN, tlsConfig.keyPassPath);
    std::copy_n(GetString(ConfConstant::OCK_MMC_HCOM_TLS_DECRYPTER_PATH).c_str(),
        TLS_PATH_MAX_LEN, tlsConfig.decrypterLibPath);
}

void Configuration::GetConfigStoreTlsConfig(tls_config &tlsConfig)
{
    tlsConfig.tlsEnable = GetBool(ConfConstant::OCK_MMC_CS_TLS_ENABLE);
    std::copy_n(GetString(ConfConstant::OCK_MMC_CS_TLS_CA_PATH).c_str(), TLS_PATH_MAX_LEN, tlsConfig.caPath);
    std::copy_n(GetString(ConfConstant::OCK_MMC_CS_TLS_CRL_PATH).c_str(), TLS_PATH_MAX_LEN, tlsConfig.crlPath);
    std::copy_n(GetString(ConfConstant::OCK_MMC_CS_TLS_CERT_PATH).c_str(), TLS_PATH_MAX_LEN, tlsConfig.certPath);
    std::copy_n(GetString(ConfConstant::OCK_MMC_CS_TLS_KEY_PATH).c_str(), TLS_PATH_MAX_LEN, tlsConfig.keyPath);
    std::copy_n(GetString(ConfConstant::OCK_MMC_CS_TLS_KEY_PASS_PATH).c_str(), TLS_PATH_MAX_LEN, tlsConfig.keyPassPath);
    std::copy_n(GetString(ConfConstant::OCK_MMC_CS_TLS_PACKAGE_PATH).c_str(), TLS_PATH_MAX_LEN, tlsConfig.packagePath);
    std::copy_n(GetString(ConfConstant::OCK_MMC_CS_TLS_DECRYPTER_PATH).c_str(),
        TLS_PATH_MAX_LEN, tlsConfig.decrypterLibPath);
}

int Configuration::ValidateTLSConfig(const tls_config &tlsConfig)
{
    if (tlsConfig.tlsEnable == false) {
        return MMC_OK;
    }

    const std::map<const char *, std::string> compulsoryMap{
        {tlsConfig.caPath, "CA(Certificate Authority) file"},
        {tlsConfig.certPath, "certificate file"},
        {tlsConfig.keyPath, "private key file"},
    };

    for (const auto &item : compulsoryMap) {
        MMC_RETURN_ERROR(ValidatePathNotSymlink(item.first), item.second << " does not exist or is a symlink");
    }

    if (!std::string(tlsConfig.crlPath).empty()) {
        MMC_RETURN_ERROR(ValidatePathNotSymlink(tlsConfig.crlPath),
            "CRL(Certificate Revocation List) file does not exist or is a symlink");
    }
    if (!std::string(tlsConfig.keyPassPath).empty()) {
        MMC_RETURN_ERROR(ValidatePathNotSymlink(tlsConfig.keyPassPath),
            "private key passphrase file does not exist or is a symlink");
    }
    if (!std::string(tlsConfig.packagePath).empty()) {
        MMC_RETURN_ERROR(ValidatePathNotSymlink(tlsConfig.packagePath),
            "openssl dynamic library directory does not exist or is a symlink");
    }
    if (!std::string(tlsConfig.decrypterLibPath).empty()) {
        MMC_RETURN_ERROR(ValidatePathNotSymlink(tlsConfig.decrypterLibPath),
            "decrypter library file does not exist or is a symlink");
    }

    return MMC_OK;
}

const std::string Configuration::GetBinDir()
{
    // 处理相对路径：获取当前可执行文件所在目录的父目录
    // 第一步：获取mmc_meta_service路径
    char pathBuf[PATH_MAX] = {0};
    ssize_t count = readlink("/proc/self/exe", pathBuf, PATH_MAX - 1);  // 预留终止符空间
    if (count == -1) {
        MMC_LOG_ERROR("mmc meta service not found bin path");
        return "";  // 错误时返回空字符串，避免后续错误
    }
    pathBuf[count] = '\0';  // 确保字符串终止

    // 第二步：获取可执行文件所在目录
    std::string binPath(pathBuf);
    size_t lastSlash = binPath.find_last_of('/');
    if (lastSlash == std::string::npos) {
        return "";  // 无效路径
    }
    std::string exeDir = binPath.substr(0, lastSlash);
    return exeDir;
}

const std::string Configuration::GetLogPath(const std::string &logPath)
{
    if (!logPath.empty() && logPath[0] == '/') {
        // 绝对路径直接返回
        return logPath;
    }

    std::string exeDir = GetBinDir();
    // 第三步：获取该目录的父目录
    size_t parentLastSlash = exeDir.find_last_of('/');
    if (parentLastSlash == std::string::npos) {
        return "/" + logPath; // 根目录下直接拼接相对路径
    }
    std::string parentDir = exeDir.substr(0, parentLastSlash);

    // 拼接相对路径（确保路径分隔符正确）
    return parentDir + "/" + logPath;
}

int Configuration::ValidateLogPathConfig(const std::string &logPath)
{
    MMC_RETURN_ERROR(ValidatePathNotSymlink(logPath.c_str()), logPath << " does not exist or is a symlink");
    return MMC_OK;
}

uint64_t Configuration::ParseMemSize(const std::string &memStr)
{
    if (memStr.empty()) {
        // UINT64_MAX代表无效值
        return UINT64_MAX;
    }
    // 查找第一个非数字字符，区分数值和单位
    size_t i = 0;
    while (i < memStr.size() && (isdigit(memStr[i]) || memStr[i] == '.')) {
        i++;
    }

    // 提取数值部分
    std::string numStr = memStr.substr(0, i);
    double num;
    try {
        num = std::stod(numStr);
    } catch (const std::exception& e) {
        return UINT64_MAX;
    }

    // 提取单位部分
    std::string unitStr = (i < memStr.size()) ? memStr.substr(i) : "b";
    OckTrimString(unitStr);
    MemUnit unit = ParseMemUnit(unitStr);

    // 转换为字节数
    uint64_t bytes;
    switch (unit) {
        case MemUnit::B:  bytes = static_cast<uint64_t>(num); break;
        case MemUnit::KB: bytes = static_cast<uint64_t>(num * KB_MEM_BYTES); break;
        case MemUnit::MB: bytes = static_cast<uint64_t>(num * MB_MEM_BYTES); break;
        case MemUnit::GB: bytes = static_cast<uint64_t>(num * GB_MEM_BYTES); break;
        case MemUnit::TB: bytes = static_cast<uint64_t>(num * TB_MEM_BYTES); break;
        default: bytes = UINT64_MAX;
    }

    return bytes;
}

// 转换字符串单位到枚举
MemUnit Configuration::ParseMemUnit(const std::string& unit)
{
    if (unit.empty()) { return MemUnit::B; }

    std::string lowerUnit = unit;
    std::transform(lowerUnit.begin(), lowerUnit.end(), lowerUnit.begin(), ::tolower);

    if (lowerUnit == "b") { return MemUnit::B; }
    if (lowerUnit == "k" || lowerUnit == "kb") { return MemUnit::KB; }
    if (lowerUnit == "m" || lowerUnit == "mb") { return MemUnit::MB; }
    if (lowerUnit == "g" || lowerUnit == "gb") { return MemUnit::GB; }
    if (lowerUnit == "t" || lowerUnit == "tb") { return MemUnit::TB; }

    return MemUnit::UNKNOWN;
}

} // namespace common
} // namespace ock