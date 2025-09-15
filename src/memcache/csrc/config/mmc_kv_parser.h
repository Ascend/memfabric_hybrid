/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */

#pragma once

#include <map>
#include <string>
#include <vector>
#include <fstream>
#include <unordered_map>

#include "mmc_lock.h"
#include "mmc_define.h"

namespace ock {
namespace mmc {
using KvPair = struct KvPairs {
    std::string name;
    std::string value;
};

class KVParser {
public:
    KVParser();
    ~KVParser();

    KVParser(const KVParser &) = delete;
    KVParser &operator = (const KVParser &) = delete;
    KVParser(const KVParser &&) = delete;
    KVParser &operator = (const KVParser &&) = delete;

    HRESULT FromFile(const std::string &filePath);

    HRESULT GetItem(const std::string &key, std::string &outValue);
    HRESULT SetItem(const std::string &key, const std::string &value);

    uint32_t Size();
    void GetI(const uint32_t index, std::string &outKey, std::string &outValue);

    void Dump();
    bool CheckSet(const std::vector<std::string> &keys);

private:
    std::map<std::string, uint32_t> mItemsIndex;
    std::vector<KvPair *> mItems;
    std::unordered_map<std::string, bool> mGotKeys;
    Lock mLock;
};
} // namespace mmc
} // namespace ock
