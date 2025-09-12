/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */

#include "mmc_kv_parser.h"

#include <linux/limits.h>

#include "mf_file_util.h"
#include "mmc_functions.h"

namespace ock {
namespace mmc {

constexpr int MAX_CONF_FILE_SIZE = 10485760;  // 10MB
constexpr uint32_t MAX_LINE_LENGTH = 4096;

KVParser::KVParser() = default;
KVParser::~KVParser()
{
    {
        GUARD(&mLock, mLock);
        for (const auto val : mItems) {
            const KvPair *p = val;
            SAFE_DELETE(p);
        }

        mItems.clear();
        mItemsIndex.clear();
    }
}

HRESULT KVParser::FromFile(const std::string &filePath)
{
    char path[PATH_MAX + 1] = {0};
    if (filePath.size() > PATH_MAX || realpath(filePath.c_str(), path) == nullptr) {
        return MMC_FAIL;
    }
    /* open file to read */
    std::ifstream inConfFile(path);
    if (!mf::FileUtil::CheckFileSize(inConfFile, MAX_CONF_FILE_SIZE)) {
        inConfFile.close();
        return MMC_FAIL;
    }
    std::string strLine;
    HRESULT hr = OK;
    while (getline(inConfFile, strLine)) {
        strLine.erase(strLine.find_last_not_of('\r') + 1);
        OckTrimString(strLine);
        if (strLine.empty() || strLine.at(0) == '#') {
            continue;
        }
        if (strLine.size() > MAX_LINE_LENGTH) {
            std::cerr << "Configuration file <" << path << "> strLine is too long." << std::endl;
            hr = MMC_FAIL;
            break;
        }

        std::string::size_type equalDivPos = strLine.find('=');
        if (equalDivPos == std::string::npos) {
            continue;
        }

        std::string strKey = strLine.substr(0, equalDivPos);
        std::string strValue = strLine.substr(equalDivPos + 1, strLine.size() - 1);
        OckTrimString(strKey);
        OckTrimString(strValue);

        if (strKey.empty()) {
            hr = MMC_FAIL;
            std::cerr << "Configuration item has empty key." << std::endl;
            break;
        }
        if (RESULT_FAIL(SetItem(strKey, strValue))) {
            hr = MMC_FAIL;
            break;
        }
    }

    inConfFile.close();
    inConfFile.clear();

    return hr;
}

HRESULT KVParser::GetItem(const std::string &key, std::string &outValue)
{
    GUARD(&mLock, mLock);
    const auto iter = mItemsIndex.find(key);
    if (iter != mItemsIndex.end()) {
        const auto itemPtr = mItems.at(iter->second);
        if (itemPtr == nullptr) {
            return MMC_FAIL;
        }
        outValue = itemPtr->value;
        return OK;
    }
    return MMC_FAIL;
}

HRESULT KVParser::SetItem(const std::string &key, const std::string &value)
{
    GUARD(&mLock, mLock);
    const auto iter = mItemsIndex.find(key);
    if (iter != mItemsIndex.end()) {
        std::cerr << "Key <" << key << "> in configuration file is repeated." << std::endl;
        return MMC_FAIL;
    }
    auto *kv = new (std::nothrow) KvPair();
    if (kv == nullptr) {
        std::cerr << "Parse lines in configuration file failed, maybe out of memory." << std::endl;
        return MMC_FAIL;
    }
    kv->name = key;
    kv->value = value;
    mItems.push_back(kv);
    mItemsIndex.insert(std::make_pair(key, mItems.size() - 1));
    if (mGotKeys.find(key) == mGotKeys.end()) {
        mGotKeys.insert(std::make_pair(key, true));
    }
    return OK;
}

uint32_t KVParser::Size()
{
    GUARD(&mLock, mLock);
    return mItems.size();
}

void KVParser::GetI(const uint32_t index, std::string &outKey, std::string &outValue)
{
    GUARD(&mLock, mLock);
    if (index >= mItems.size()) {
        return;
    }
    const auto itemPtr = mItems.at(index);
    if (itemPtr == nullptr) {
        return;
    }
    outKey = itemPtr->name;
    outValue = itemPtr->value;
}

void KVParser::Dump()
{
    GUARD(&mLock, mLock);
    for (const auto val : mItems) {
        const KvPair *p = val;
        if (p == nullptr) {
            continue;
        }
        printf("%s = %s\n", p->name.c_str(), p->value.c_str());
    }
}

bool KVParser::CheckSet(const std::vector<std::string> &keys)
{
    bool check = true;
    for (auto &item : keys) {
        if (mGotKeys.find(item) == mGotKeys.end()) {
            std::cerr << "Config item <" << item << "> is not set!" << std::endl;
            check = false;
        }
    }
    mGotKeys = std::unordered_map<std::string, bool>();
    return check;
}
} // namespace mmc
} // namespace ock
