/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

#include <bits/fs_fwd.h>
#include <sys/mman.h>
#include <unistd.h>
#include <cassert>
#include <iostream>
#include <cstring>
#include <sstream>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iomanip>
#include <iostream>
#include <random>
#include <signal.h>
#include "mmcache.h"
#include "mmc_logger.h"
#include "mf_str_util.h"

using namespace ock::mmc;

namespace ock::mmc::cpp::test {
constexpr auto DEFAULT_KEY = "test1";
constexpr size_t BUFFER_SIZE = 2ULL * 1024 * 1024; // 2MB
constexpr uint64_t FNV_OFFSET_BASIS = 14695981039346656037ULL;
constexpr uint64_t FNV_PRIME = 1099511628211ULL;

class MemoryManager {
public:
    explicit MemoryManager(const size_t size) : size_(size), data_(std::malloc(size))
    {
        if (!data_) {
            throw std::bad_alloc();
        }
        initializeWithPattern();
    }

    ~MemoryManager()
    {
        if (data_) {
            std::free(data_);
        }
    }

    MemoryManager(const MemoryManager &) = delete;
    MemoryManager &operator=(const MemoryManager &) = delete;

    MemoryManager(MemoryManager &&other) noexcept : size_(other.size_), data_(other.data_)
    {
        other.data_ = nullptr;
        other.size_ = 0;
    }

    MemoryManager &operator=(MemoryManager &&other) noexcept
    {
        if (this != &other) {
            if (data_) {
                std::free(data_);
            }
            size_ = other.size_;
            data_ = other.data_;
            other.data_ = nullptr;
            other.size_ = 0;
        }
        return *this;
    }

    void *get() const
    {
        return data_;
    }
    size_t size() const
    {
        return size_;
    }

private:
    void initializeWithPattern() const
    {
        auto *memory = static_cast<uint8_t *>(data_);
        for (size_t i = 0; i < size_; ++i) {
            memory[i] = i % std::numeric_limits<uint8_t>::max();
        }
    }

    size_t size_;
    void *data_;
};

class FNVHashCalculator {
public:
    static uint64_t calculate(const MemoryManager &memory)
    {
        const auto data = memory.get();
        const auto size = memory.size();
        if (data == nullptr || size == 0) {
            return 0;
        }
        const auto *blocks = static_cast<const uint64_t *>(data);
        const size_t block_count = size / sizeof(uint64_t);
        uint64_t hash = FNV_OFFSET_BASIS;

        for (size_t i = 0; i < block_count; ++i) {
            hash ^= blocks[i];
            hash *= FNV_PRIME;
        }

        const uint8_t *tail = static_cast<const uint8_t *>(data) + block_count * sizeof(uint64_t);
        const size_t remaining_bytes = size % sizeof(uint64_t);

        for (size_t i = 0; i < remaining_bytes; ++i) {
            hash ^= static_cast<uint64_t>(tail[i]);
            hash *= FNV_PRIME;
        }
        return hash;
    }
};

class StoreOperator {
public:
    StoreOperator(std::shared_ptr<ObjectStore> store, const size_t buffer_size)
        : store_(std::move(store)), buffer_size_(buffer_size), put_buffer_(buffer_size), get_buffer_(buffer_size),
          put_hash_value_(FNVHashCalculator::calculate(put_buffer_))
    {
        MMC_LOG_INFO("Initialized with put hash value: " << put_hash_value_);
    }

    bool performOperation() const
    {
        std::string operation;
        std::string key;

        std::cin >> operation;
        if (operation == "exit") {
            return true;
        }

        std::cin >> key;
        if (key.empty()) {
            std::cerr << "Error: Key cannot be empty" << std::endl;
            return false;
        }

        if (operation == "get") {
            handleGet(key);
        } else if (operation == "remove") {
            handleRemove(key);
        } else if (operation == "put") {
            handlePut(key);
        } else {
            std::cerr << "Error: Unknown operation: " << operation << std::endl;
            printUsage();
        }

        return false;
    }

    void initializeDefaultData()
    {
        MMC_LOG_INFO("Checking if key exists: " << DEFAULT_KEY);

        if (store_->IsExist(DEFAULT_KEY) == 0) {
            const int result = store_->PutFrom(DEFAULT_KEY, put_buffer_.get(), buffer_size_);
            MMC_LOG_INFO("Initial PutFrom result: " << result);
            if (result != 0) {
                throw std::runtime_error("Failed to put initial data");
            }
        }

        const auto batch_result = store_->BatchPutFrom({"kBatchPutFrom"}, {put_buffer_.get()}, {buffer_size_});
        MMC_LOG_INFO("BatchPutFrom result: " << batch_result[0]);
        store_->Remove("kBatchPutFrom");

        if (store_->IsExist(DEFAULT_KEY) != 0) {
            verifyRetrievedData(DEFAULT_KEY);
        }
    }

    uint64_t getHashValue() const
    {
        return put_hash_value_;
    }

private:
    void handleGet(const std::string &key) const
    {
        MMC_LOG_INFO("Checking existence for key: " << key);
        if (store_->IsExist(key) != 0) {
            const int result = store_->GetInto(key, get_buffer_.get(), buffer_size_);
            MMC_LOG_INFO("GetInto result: " << result);

            if (result == 0) {
                verifyRetrievedData(key);
            }
        } else {
            MMC_LOG_INFO("Key does not exist: " << key);
        }
    }

    void handleRemove(const std::string &key) const
    {
        const int result = store_->Remove(key);
        MMC_LOG_INFO("Remove result: " << result << " for key: " << key);
    }

    void handlePut(const std::string &key) const
    {
        int replica_num = 1;
        if (!(std::cin >> replica_num)) {
            std::cerr << "Error: Invalid replica number" << std::endl;
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            return;
        }

        ReplicateConfig replicate_config{};
        replicate_config.replicaNum = replica_num;

        const int result = store_->PutFrom(key, put_buffer_.get(), buffer_size_, 3, replicate_config);
        MMC_LOG_INFO("PutFrom result: " << result << ", key: " << key << ", replicas: " << replicate_config.replicaNum);
    }

    void verifyRetrievedData(const std::string &key) const
    {
        const uint64_t retrieved_hash = FNVHashCalculator::calculate(get_buffer_);
        MMC_LOG_INFO("Retrieved data hash for key " << key << ": " << retrieved_hash);

        if (retrieved_hash != put_hash_value_) {
            std::cerr << "Error: Hash mismatch for key " << key << ". Expected: " << put_hash_value_
                      << ", Got: " << retrieved_hash << std::endl;
            throw std::runtime_error("Data verification failed");
        }
    }

    static void printUsage()
    {
        std::cout << "Available operations:\n"
                  << "  get <key>     - Retrieve data for key\n"
                  << "  put <key> <replicas> - Store data with replication\n"
                  << "  remove <key>  - Remove data for key\n"
                  << "  exit          - Exit program\n";
    }

    std::shared_ptr<ObjectStore> store_;
    size_t buffer_size_;
    MemoryManager put_buffer_;
    MemoryManager get_buffer_;
    uint64_t put_hash_value_;
};
static std::shared_ptr<ObjectStore> g_store{nullptr};

void SignalInterruptHandler(int signal)
{
    std::cout << "Received exit signal[" << signal << "]" << std::endl;
    if (g_store) {
        g_store->TearDown();
    }
}

void RegisterSignal()
{
    const sighandler_t oldIntHandler = signal(SIGINT, SignalInterruptHandler);
    if (oldIntHandler == SIG_ERR) {
        std::cerr << "Register SIGINT handler failed" << std::endl;
    }

    const sighandler_t oldTermHandler = signal(SIGTERM, SignalInterruptHandler);
    if (oldTermHandler == SIG_ERR) {
        std::cerr << "Register SIGTERM handler failed" << std::endl;
    }
}

static std::vector<std::string> executeCmd(const std::string &command)
{
    std::vector<std::string> outputLines;
    if (command.empty()) {
        throw std::invalid_argument("命令不能为空");
    }

    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command.c_str(), "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("无法执行命令: " + command);
    }

    char buffer[256U];
    while (fgets(buffer, sizeof(buffer), pipe.get()) != nullptr) {
        buffer[255U] = '\0';
        auto line = std::string(buffer);
        if (!line.empty()) {
            outputLines.push_back(line);
        }
    }

    int returnCode = pclose(pipe.release());
    if (returnCode != 0) {
        throw std::runtime_error("命令执行失败，返回码: " + std::to_string(returnCode));
    }

    return outputLines;
}
} // namespace ock::mmc::cpp::test

using namespace ock::mmc::cpp::test;

int main(int argc, char *argv[])
{
    try {
        (void)argc;
        (void)argv;
        auto list = executeCmd("npu-smi info | grep 'No running processes'");
        for (const auto &line : list) {
            std::cout << line << std::endl;
        }
        uint32_t device_id = 0;
        for (int i = list.size(); i > 0; --i) {
            auto it =
                std::find_if(list[i - 1].begin(), list[i - 1].end(), [](const char c) { return std::isdigit(c); });
            uint32_t tmp = 0;
            ock::mf::StrUtil::String2Uint(std::string(1, *it), tmp);
            if (tmp > 0) {
                std::cout << "change device id to " << tmp << std::endl;
                device_id = tmp;
                break;
            }
        }
        const auto store = ObjectStore::CreateObjectStore();
        if (!store) {
            std::cerr << "Error: Failed to create object store" << std::endl;
            return -1;
        }
        g_store = store;
        if (store->Init(device_id) != 0) {
            std::cerr << "Error: Failed to initialize MMC Store" << std::endl;
            return -1;
        }
        StoreOperator operator_(store, BUFFER_SIZE);
        operator_.initializeDefaultData();

        std::cout << "Store operations ready. Type 'exit' to quit.\n";
        while (!operator_.performOperation()) {}

        std::cout << "Exiting program." << std::endl;
        SignalInterruptHandler(0);
        return 0;
    } catch (const std::bad_alloc &) {
        std::cerr << "Error: Memory allocation failed" << std::endl;
    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "Error: Unknown exception occurred" << std::endl;
    }
    SignalInterruptHandler(0);
    return -1;
}