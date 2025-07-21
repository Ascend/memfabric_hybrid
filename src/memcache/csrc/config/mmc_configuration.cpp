/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */

#include "mmc_configuration.h"

#include <iostream>

#include "mmc_kv_parser.h"

namespace ock {
namespace mmc {

static constexpr int MAX_CONF_ITEM_COUNT = 100;

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
    if (RESULT_FAIL(kvParser->FromFile(filePath))) {
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

bool Configuration::GetBool(const std::pair<const char *, bool> &item) {
    GUARD(&mLock, mLock);
    const auto iter = mBoolItems.find(item.first);
    if (iter != mBoolItems.end()) {
        return iter->second;
    }
    return item.second;
}

uint64_t Configuration::GetUInt64(const std::pair<const char *, uint64_t> &item) {
    GUARD(&mLock, mLock);
    const auto iter = mUInt64Items.find(item.first);
    if (iter != mUInt64Items.end()) {
        return iter->second;
    }
    return item.second;
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
        if (mIntItems.count(key) > 0) {
            mIntItems.at(key) = static_cast<int32_t>(tmp);
        }
    } else if (valueType == ConfValueType::VFLOAT) {
        if (!OckStof(value, mFloatItems.at(key))) {
            std::cerr << "<" << key << "> was empty or in wrong type, it should be a float number." << std::endl;
            return false;
        }
    } else if (valueType == ConfValueType::VSTRING) {
        if (mStrItems.count(key) > 0) {
            std::string tempValue = value;
            if (find(mPathConfs.begin(), mPathConfs.end(), key) != mPathConfs.end() &&
                !GetRealPath(tempValue)) { // 简化路径为绝对路径
                std::cerr << "Simplify <" << key << "> to absolute path failed." << std::endl;
                return false;
            }
            mStrItems.at(key) = tempValue;
        }
    } else if (valueType == ConfValueType::VBOOL) {
        bool b = false;
        if (!IsBool(value, b)) {
            std::cerr << "<" << key << "> should represent a bool value." << std::endl;
            return false;
        }
        if (mBoolItems.count(key) > 0) {
            mBoolItems.at(key) = b;
        }
    } else if (valueType == ConfValueType::VUINT64) {
        uint64_t tmp = 0;
        if (!OckStoULL(value, tmp)) {
            std::cerr << "<" << key << "> was empty or in wrong type, it should be a unsigned long long number." << std::endl;
            return false;
        }
        if (mUInt64Items.count(key) > 0) {
            mUInt64Items.at(key) = tmp;
        }
    }
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
    auto typeIter = mValueTypes.find(itemKey);
    if (validatorIter == mValueValidator.end()) {
        errors.push_back("Failed to find <" + itemKey + "> in Validator map, which should not happen.");
        return;
    }
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
    for (auto validate : mValueValidator) {
        if (validate.second == nullptr) {
            std::string errorInfo = "The validator of <" + validate.first + "> create failed, maybe out of memory.";
            errors.push_back(errorInfo);
            continue;
        }
        ValidateItem(validate.first, errors);
    }
    return errors;
}


std::vector<std::string> Configuration::Validate(bool isAuth, bool isTLS, bool isAuthor, bool isZKSecure)
{
    std::vector<std::string> errors;
    ValidateOneValueMap(errors, mValueValidator);

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

void Configuration::GetTlsConfig(mmc_tls_config &tlsConfig)
{
    tlsConfig.tlsEnable = GetBool(ConfConstant::OCK_MMC_TLS_ENABLE);
    strncpy(tlsConfig.tlsTopPath, GetString(ConfConstant::OCK_MMC_TLS_TOP_PATH).c_str(), PATH_MAX_SIZE);
    strncpy(tlsConfig.tlsCaPath, GetString(ConfConstant::OCK_MMC_TLS_CA_PATH).c_str(), PATH_MAX_SIZE);
    strncpy(tlsConfig.tlsCertPath, GetString(ConfConstant::OCK_MMC_TLS_CERT_PATH).c_str(), PATH_MAX_SIZE);
    strncpy(tlsConfig.tlsKeyPath, GetString(ConfConstant::OCK_MMC_TLS_KEY_PATH).c_str(), PATH_MAX_SIZE);
    strncpy(tlsConfig.packagePath, GetString(ConfConstant::OCK_MMC_TLS_PACKAGE_PATH).c_str(), PATH_MAX_SIZE);
}
} // namespace common
} // namespace ock