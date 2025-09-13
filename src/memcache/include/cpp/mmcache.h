/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef __MM_CACHE_H__
#define __MM_CACHE_H__

#include <string>
#include <vector>
#include <iostream>
#include <memory>

namespace ock {
namespace mmc {

class KeyInfo {
public:
    KeyInfo(uint64_t size, uint32_t blobNum) : size_(size), blobNum_(blobNum) {};

    ~KeyInfo() = default;

    uint64_t Size()
    {
        return size_;
    }

    uint32_t GetBlobNum()
    {
        return blobNum_;
    }

    std::vector<int> &GetLocs()
    {
        return loc_;
    }

    std::vector<int> &GetTypes()
    {
        return type_;
    }

    void AddType(int type)
    {
        type_.emplace_back(type);
    }

    void AddLoc(int loc)
    {
        loc_.emplace_back(loc);
    }

    std::string ToString() const
    {
        std::stringstream desc;
        desc << "loc:";
        for (auto loc : loc_) {
            desc << std::to_string(loc) << ",";
        }
        desc << " type:";
        for (auto type : type_) {
            desc << std::to_string(type) << ",";
        }
        desc << " blobNum:" << blobNum_;
        desc << ", size:" << size_;
        return desc.str();
    }

    friend std::ostream &operator<<(std::ostream &os, const KeyInfo &keyInfo)
    {
        os << "keyinfo{" << keyInfo.ToString() << "}";
        return os;
    }

private:
    uint64_t size_{};
    uint32_t blobNum_{};
    std::vector<int> loc_{};   // blob's location
    std::vector<int> type_{};  // blob's media type
};

class ObjectStore {
public:
    ObjectStore() = default;
    virtual ~ObjectStore() = 0;

    // Deleted copy constructor and assignment (non-copyable by default)
    ObjectStore(const ObjectStore &) = delete;
    ObjectStore &operator=(const ObjectStore &) = delete;

    // Allow move semantics if needed (optional, can be deleted too)
    ObjectStore(ObjectStore &&) noexcept = default;
    ObjectStore &operator=(ObjectStore &&) noexcept = default;

    /**
     * @brief create a default object store
     * @return not null on success, null on error
     */
    static std::shared_ptr<ObjectStore> CreateObjectStore();

    /**
     * @brief init the object store
     * @param deviceId the device id where the process is located
     * @return zero on success, other on error
     */
    virtual int Init(const uint32_t deviceId) = 0;

    /**
     * @brief uninit the object store
     * @return 0 on success, other on error
     */
    virtual int TearDown() = 0;

    /**
     * @brief register buffer for zero-copy operations
     * @param buffer Pointer to the pre-allocated buffer
     * @param size Number of bytes
     * @return zero on success, other on error
     */
    virtual int RegisterBuffer(void *buffer, size_t size) = 0;

    /**
     * @brief Get object data directly into a pre-allocated buffer
     * @param key Key of the object to get
     * @param buffer Pointer to the pre-allocated buffer
     * @param size size of the buffer
     * @param direct direct indicate the location of the buffer, for detailed meanings, refer to smem_bm_copy_type
     * @return zero on success, other on error
     */
    virtual int GetInto(const std::string &key, uint8_t *buffer, size_t size, const int32_t direct) = 0;

    /**
     * @brief Get object data directly into pre-allocated buffers for multiple
     * keys (batch version)
     * @param keys Vector of keys of the objects to get
     * @param buffers Vector of pointers to the pre-allocated buffers
     * @param sizes Vector of sizes of the buffers
     * @param direct direct indicate the location of the data, for detailed meanings, refer to smem_bm_copy_type
     * @return Vector of integers, where each element is the number of bytes
     * read on success, or a negative value on error
     */
    virtual std::vector<int> BatchGetInto(const std::vector<std::string> &keys, const std::vector<void *> &buffers,
                                          const std::vector<size_t> &sizes, const int32_t direct) = 0;

    /**
     * @brief Get object data directly into a pre-allocated buffer
     * @param key Key of the object to get
     * @param buffer Pointer to the pre-allocated buffers, which belongs to the key
     * @param sizes Vector of sizes of the buffers
     * @param direct direct indicate the location of the data, for detailed meanings, refer to smem_bm_copy_type
     * @return zero on success, other on error
     */
    virtual int GetIntoLayers(const std::string &key, const std::vector<uint8_t *> &buffers,
                              const std::vector<size_t> &sizes, const int32_t direct) = 0;

    /**
     * @brief Get object data directly into pre-allocated buffers for multiple
     * keys (batch version)
     * @param keys Vector of keys of the objects to get
     * @param buffers Vector of pointers to the pre-allocated buffers
     * @param sizes Vector of sizes of the buffers
     * @param direct direct indicate the location of the data, for detailed meanings, refer to smem_bm_copy_type
     * @return Vector of integers, where each element is the number of bytes
     * read on success, or a negative value on error
     */
    virtual std::vector<int> BatchGetIntoLayers(const std::vector<std::string> &keys,
                                                const std::vector<std::vector<uint8_t *>> &buffers,
                                                const std::vector<std::vector<size_t>> &sizes,
                                                const int32_t direct) = 0;

    /**
     * @brief Put object data directly from a pre-allocated buffer
     * @param key Key of the object to put
     * @param buffer Pointer to the buffer containing data
     * @param size size of the buffer
     * @param direct direct indicate the location of the buffer, for detailed meanings, refer to smem_bm_copy_type
     * @return 0 on success, negative value on error
     */
    virtual int PutFrom(const std::string &key, uint8_t *buffer, size_t size, const int32_t direct) = 0;

    /**
     * @brief Put object data directly from pre-allocated buffers for multiple
     * keys (batch version)
     * @param keys Vector of keys of the objects to put
     * @param buffers Vector of pointers to the pre-allocated buffers
     * @param sizes Vector of sizes of the buffers
     * @param direct direct indicate the location of the data, for detailed meanings, refer to smem_bm_copy_type
     * @return Vector of integers, where each element is 0 on success, or a
     * negative value on error
     */
    virtual std::vector<int> BatchPutFrom(const std::vector<std::string> &keys, const std::vector<void *> &buffers,
                                          const std::vector<size_t> &sizes, const int32_t direct) = 0;

    /**
     * @brief Put object data directly from a pre-allocated buffer
     * @param key Key of the object to put
     * @param buffers Pointer to the buffer containing data
     * @param sizes Vector of sizes of the buffers
     * @param direct direct indicate the location of the data, for detailed meanings, refer to smem_bm_copy_type
     * @return 0 on success, negative value on error
     */
    virtual int PutFromLayers(const std::string &key, const std::vector<uint8_t *> &buffers,
                              const std::vector<size_t> &sizes, const int32_t direct) = 0;

    /**
     * @brief Put object data directly from pre-allocated buffers for multiple
     * keys (batch version)
     * @param keys Vector of keys of the objects to put
     * @param buffers Vector of pointers to the pre-allocated buffers
     * @param sizes Vector of sizes of the buffers
     * @param direct direct indicate the location of the data, for detailed meanings, refer to smem_bm_copy_type
     * @return Vector of integers, where each element is 0 on success, or a
     * negative value on error
     */
    virtual std::vector<int> BatchPutFromLayers(const std::vector<std::string> &keys,
                                                const std::vector<std::vector<uint8_t *>> &buffers,
                                                const std::vector<std::vector<size_t>> &sizes,
                                                const int32_t direct) = 0;

    /**
     * @brief remove the object
     * @param key Key of the object to remove
     * @return 0 on success, negative value on error
     */
    virtual int Remove(const std::string &key) = 0;

    /**
     * @brief Remove objects with keys
     * @param keys Keys of the objects
     * @return std::vector<int> Remove status for each keys, 1 if success, -1 if error
     * exist
     */
    virtual std::vector<int> BatchRemove(const std::vector<std::string> &keys) = 0;

    /**
     * @brief Check if an object exists
     * @param key Key to check
     * @return 1 if exists, 0 if not exists, -1 if error
     */
    virtual int IsExist(const std::string &key) = 0;

    /**
     * @brief Check if multiple objects exist
     * @param keys Vector of keys to check
     * @return Vector of existence results: 1 if exists, 0 if not exists, -1 if
     * error
     */
    virtual std::vector<int> BatchIsExist(const std::vector<std::string> &keys) = 0;

    /**
     * @brief get object info
     * @param keys key to get info
     * @return key info
     */
    virtual KeyInfo GetKeyInfo(const std::string &key) = 0;

    /**
     * @brief get multiple objects info
     * @param keys Vector of keys to get info
     * @return Vector of key infos
     */
    virtual std::vector<KeyInfo> BatchGetKeyInfo(const std::vector<std::string> &keys) = 0;
};

}  // namespace mmc
}  // namespace ock
#endif