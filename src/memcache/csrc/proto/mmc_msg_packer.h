/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#ifndef MEMFABRIC_MMC_MSG_PACKER_H
#define MEMFABRIC_MMC_MSG_PACKER_H

#include "mmc_common_includes.h"

namespace ock {
namespace mmc {
class NetMsgPacker {
public:
    /**
     * @brief Append a POD(Plain Old Data) struct
     *
     * @tparam T           [in] type of POD
     * @param val          [in] value of POD
     */
    template <typename T>
    void Serialize(const T &val, typename std::enable_if<std::is_trivially_copyable<T>::value, int>::type = 0)
    {
        outStream_.write(reinterpret_cast<const char *>(&val), sizeof(T));
    }

    /**
     * @brief Append a string
     *
     * @param val          [in] value of string
     */
    void Serialize(const std::string &val)
    {
        uint32_t size = val.size();
        outStream_.write(reinterpret_cast<const char *>(&size), sizeof(size));
        outStream_.write(val.data(), size);
    }

    /**
     * @brief Append a pair
     *
     * @tparam K           [in] type of K of pair
     * @tparam V           [in] type of V of pair
     * @param val          [in] value of pair
     */
    template <typename K, typename V>
    void Serialize(const std::pair<K, V> &val)
    {
        Serialize(val.first);
        Serialize(val.second);
    }

    /**
     * @brief Append a vector
     *
     * @tparam V           [in] type of vector element
     * @param container    [in] vector to be appended
     */
    template <typename V>
    void Serialize(const std::vector<V> &container)
    {
        const std::size_t size = container.size();
        outStream_.write(reinterpret_cast<const char *>(&size), sizeof(size));
        for (const auto &item : container) {
            Serialize(item);
        }
    }

    /**
     * @brief Append a map
     *
     * @tparam K           [in] type of map key
     * @tparam V           [in] type of map value
     * @param container    [in] map to be appended
     */
    template <typename K, typename V>
    void Serialize(const std::map<K, V> &container)
    {
        const std::size_t size = container.size();
        outStream_.write(reinterpret_cast<const char *>(&size), sizeof(size));
        for (const auto &item : container) {
            Serialize(item);
        }
    }

    /**
     * @brief Get the serialized result
     *
     * @return String value of serialized
     */
    std::string String() const
    {
        return outStream_.str();
    }

private:
    std::ostringstream outStream_;
};

class NetMsgUnpacker {
public:
    /**
     * @brief Create unpacker with serialized data
     *
     * @param value        [in] serialized data with string type
     */
    explicit NetMsgUnpacker(const std::string &value) : inStream_(value) {}

    /**
     * @brief Take data and deserialize to POD data
     *
     * @tparam T           [in] type of POD
     * @param val          [in/out] result data of POD
     */
    template <typename T>
    void Deserialize(T &val, typename std::enable_if<std::is_trivially_copyable<T>::value, int>::type = 0)
    {
        inStream_.read(reinterpret_cast<char *>(&val), sizeof(T));
    }

    /**
     * @brief Take data and deserialize to string
     *
     * @param val          [in/out] result data of string
     */
    void Deserialize(std::string &val)
    {
        uint32_t size = 0;
        inStream_.read(reinterpret_cast<char *>(&size), sizeof(size));
        val.resize(size);
        inStream_.read(&val[0], size);
    }

    /**
     * @brief Take data and deserialize to vector
     *
     * @tparam V           [in] type of vector element
     * @param container    [in/out] result data of vector
     */
    template <typename V>
    void Deserialize(std::vector<V> &container)
    {
        std::size_t size = 0;
        inStream_.read(reinterpret_cast<char *>(&size), sizeof(size));
        container.clear();
        container.reserve(size);
        for (std::size_t i = 0; i < size; ++i) {
            V item;
            Deserialize(item);
            container.emplace_back(std::move(item));
        }
    }

    /**
     * @brief Take data and deserialize to vector
     *
     * @tparam K           [in] type of map key
     * @tparam V           [in] type of map value
     * @param container    [in/out] result data of map
     */
    template <typename K, typename V>
    void Deserialize(std::map<K, V> &container)
    {
        std::size_t size = 0;
        inStream_.read(reinterpret_cast<char *>(&size), sizeof(size));
        container.clear();
        for (std::size_t i = 0; i < size; ++i) {
            K key;
            V value;
            Deserialize(key);
            Deserialize(value);
            container.emplace(std::move(key), std::move(value));
        }
    }

private:
    std::istringstream inStream_;
};
}
}

#endif  //MEMFABRIC_MMC_MSG_PACKER_H
