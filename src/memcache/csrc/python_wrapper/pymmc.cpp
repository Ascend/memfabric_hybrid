/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */

#include "pymmc.h"

#include <iostream>

#include "mmc_client.h"
#include "mmc.h"
#include "mmc_logger.h"
#include "mmc_define.h"
#include "mmc_types.h"
#include "mmc_last_error.h"

namespace py = pybind11;
using namespace ock::mmc;

// ResourceTracker implementation using singleton pattern
ResourceTracker &ResourceTracker::getInstance() {
    static ResourceTracker instance;
    return instance;
}

ResourceTracker::ResourceTracker() {
    // Set up signal handlers
    struct sigaction sa;
    sa.sa_handler = signalHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    // Register for common termination signals
    sigaction(SIGINT, &sa, nullptr);   // Ctrl+C
    sigaction(SIGTERM, &sa, nullptr);  // kill command
    sigaction(SIGHUP, &sa, nullptr);   // Terminal closed

    // Register exit handler
    std::atexit(exitHandler);
}

ResourceTracker::~ResourceTracker() {
    // Cleanup is handled by exitHandler
}

void ResourceTracker::registerInstance(DistributedObjectStore *instance) {
    std::lock_guard<std::mutex> lock(mutex_);
    instances_.insert(instance);
}

void ResourceTracker::unregisterInstance(DistributedObjectStore *instance) {
    std::lock_guard<std::mutex> lock(mutex_);
    instances_.erase(instance);
}

void ResourceTracker::cleanupAllResources() {
    std::lock_guard<std::mutex> lock(mutex_);

    // Perform cleanup outside the lock to avoid potential deadlocks
    for (void *instance : instances_) {
        auto *store = static_cast<DistributedObjectStore *>(instance);
        if (store) {
            std::cout << "Cleaning up DistributedObjectStore instance" << std::endl;
            store->tearDownAll();
        }
    }
}

void ResourceTracker::signalHandler(int signal) {
    std::cout << "Received signal " << signal << ", cleaning up resources" << std::endl;
    getInstance().cleanupAllResources();

    // Re-raise the signal with default handler to allow normal termination
    struct sigaction sa;
    sa.sa_handler = SIG_DFL;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(signal, &sa, nullptr);
    raise(signal);
}

void ResourceTracker::exitHandler() { getInstance().cleanupAllResources(); }

DistributedObjectStore::DistributedObjectStore() {
    // Register this instance with the global tracker
    // easylog::set_min_severity(easylog::Severity::WARN);
    ResourceTracker::getInstance().registerInstance(this);
}

DistributedObjectStore::~DistributedObjectStore() {
    // Unregister from the tracker before cleanup
    ResourceTracker::getInstance().unregisterInstance(this);
}

int DistributedObjectStore::init() {
    return mmc_init();
}

int DistributedObjectStore::setup(const std::string &local_hostname,
                                  const std::string &metadata_server,
                                  size_t global_segment_size,
                                  size_t local_buffer_size,
                                  const std::string &protocol,
                                  const std::string &rdma_devices,
                                  const std::string &master_server_addr) {
    return 0;
}

int DistributedObjectStore::initAll(const std::string &protocol,
                                    const std::string &device_name,
                                    size_t mount_segment_size) {
    return 0;
}

int DistributedObjectStore::tearDownAll() {
    mmc_uninit();
    return 0;
}

int DistributedObjectStore::put(const std::string &key, mmc_buffer &buffer) {
    mmc_put_options options = {.mediaType = 0, .policy = NATIVE_AFFINITY};
    return mmcc_put(key.c_str(), &buffer, options, 0);
}

int DistributedObjectStore::put_batch(
    const std::vector<std::string> &keys,
    const std::vector<mmc_buffer> &values) {
    return 0;
}

int DistributedObjectStore::put_parts(const std::string &key, std::vector<mmc_buffer> values) {
    return 0;
}

pybind11::bytes DistributedObjectStore::get(const std::string &key) {
    mmc_data_info info;
    auto res = mmcc_query(key.c_str(), &info, 0);
    if (res != MMC_OK) {
        MMC_LOG_ERROR("Failed to query key " << key << ", error code: " << res);
        py::gil_scoped_acquire acquire_gil;
        return {"\0", 0};
    }

    const auto data_ptr = std::shared_ptr<char[]>(new char[info.size]);
    if (data_ptr == nullptr) {
        MMC_LOG_ERROR("Failed to allocate dynamic memory. ");
        py::gil_scoped_acquire acquire_gil;
        return {"\0", 0};
    }
    mmc_buffer buffer = {
        .addr = reinterpret_cast<uintptr_t>(data_ptr.get()),
        .type = 0,
        .dram = {
            .offset = 0,
            .len = info.size,
        }
    };

    res = mmcc_get(key.c_str(), &buffer, 0);
    if (res != MMC_OK) {
        MMC_LOG_ERROR("Failed to get key " << key << ", error code: " << res);
        py::gil_scoped_acquire acquire_gil;
        return {"\0", 0};
    }

    py::gil_scoped_acquire acquire_gil;
    return {reinterpret_cast<const char *>(buffer.addr), buffer.dram.len};
}

std::vector<pybind11::bytes> DistributedObjectStore::get_batch(const std::vector<std::string> &keys) {
    const auto kNullString = pybind11::bytes("\0", 0);
    return {kNullString};
}

int DistributedObjectStore::remove(const std::string &key) {
    return mmcc_remove(key.c_str(), 0);
}

long DistributedObjectStore::removeAll() {
    return 0;
}

int DistributedObjectStore::isExist(const std::string &key) {
    int32_t res = mmcc_exist(key.c_str(), 0);
    if (res == 0) {
        // align with mooncake: 1 represents exist
        return 1;
    }
    // align with mooncake: 0 represents not exist
    return 0;
}

std::vector<int> DistributedObjectStore::batchIsExist(const std::vector<std::string> &keys) {
    std::vector<int> results;
    return results;
}

int64_t DistributedObjectStore::getSize(const std::string &key) {
    return 0;
}

// SliceBuffer implementation
SliceBuffer::SliceBuffer(DistributedObjectStore &store, void *buffer,
                         uint64_t size, bool use_allocator_free)
    : store_(store),
      buffer_(buffer),
      size_(size),
      use_allocator_free_(use_allocator_free) {}

SliceBuffer::~SliceBuffer() {
    // if (buffer_) {
    //     if (use_allocator_free_) {
    //         // Use SimpleAllocator to deallocate memory
    //         store_.client_buffer_allocator_->deallocate(buffer_, size_);
    //     } else {
    //         // Use delete[] for memory allocated with new[]
    //         delete[] static_cast<char *>(buffer_);
    //     }
    //     buffer_ = nullptr;
    // }
}

void *SliceBuffer::ptr() const { return buffer_; }

uint64_t SliceBuffer::size() const { return size_; }

// Implementation of get_buffer method
std::shared_ptr<SliceBuffer> DistributedObjectStore::get_buffer(const std::string &key) {
    return nullptr;
}

int DistributedObjectStore::register_buffer(void *buffer, size_t size) {
    return 0;
}

int DistributedObjectStore::get_into(const std::string &key, void *buffer, size_t size) {
    return 0;
}

std::vector<int> DistributedObjectStore::batch_put_from(
    const std::vector<std::string> &keys, const std::vector<void *> &buffers,
    const std::vector<size_t> &sizes) {
    std::vector<int> results(keys.size());
    return results;
}

std::vector<int> DistributedObjectStore::batch_get_into(
    const std::vector<std::string> &keys, const std::vector<void *> &buffers,
    const std::vector<size_t> &sizes) {
    std::vector<int> results(keys.size());
    return results;
}

int DistributedObjectStore::put_from(const std::string &key, void *buffer, size_t size) {
    return 0;
}

PYBIND11_MODULE(_pymmc, m) {
    // Define the SliceBuffer class
    py::class_<SliceBuffer, std::shared_ptr<SliceBuffer>>(m, "SliceBuffer",
                                                          py::buffer_protocol())
        .def("ptr",
             [](const SliceBuffer &self) {
                 // Return the pointer as an integer for Python
                 return reinterpret_cast<uintptr_t>(self.ptr());
             })
        .def("size", &SliceBuffer::size)
        .def("__len__", &SliceBuffer::size)
        .def_buffer([](SliceBuffer &self) -> py::buffer_info {
            // SliceBuffer now always contains contiguous memory
            if (self.size() > 0) {
                return py::buffer_info(
                    self.ptr(),   /* Pointer to buffer */
                    sizeof(char), /* Size of one scalar */
                    py::format_descriptor<
                        char>::format(),   /* Python struct-style
                                              format descriptor */
                    1,                     /* Number of dimensions */
                    {(size_t)self.size()}, /* Buffer dimensions */
                    {sizeof(char)} /* Strides (in bytes) for each index */
                );
            } else {
                // Empty buffer
                return py::buffer_info(
                    nullptr,      /* Pointer to buffer */
                    sizeof(char), /* Size of one scalar */
                    py::format_descriptor<
                        char>::format(), /* Python struct-style
                                            format descriptor */
                    1,                   /* Number of dimensions */
                    {0},                 /* Buffer dimensions */
                    {sizeof(char)}       /* Strides (in bytes) for each index */
                );
            }
        });

    // Define the DistributedObjectStore class
    py::class_<DistributedObjectStore>(m, "DistributedObjectStore")
        .def(py::init<>())
        .def("init", &DistributedObjectStore::init)
        .def("setup", &DistributedObjectStore::setup)
        .def("init_all", &DistributedObjectStore::initAll)
        .def("get", &DistributedObjectStore::get)
        .def("get_batch", &DistributedObjectStore::get_batch)
        .def("get_buffer", &DistributedObjectStore::get_buffer,
             py::call_guard<py::gil_scoped_release>(),
             py::return_value_policy::take_ownership)
        .def("remove", &DistributedObjectStore::remove,
             py::call_guard<py::gil_scoped_release>())
        .def("remove_all", &DistributedObjectStore::removeAll,
             py::call_guard<py::gil_scoped_release>())
        .def("is_exist", &DistributedObjectStore::isExist,
             py::call_guard<py::gil_scoped_release>())
        .def("batch_is_exist", &DistributedObjectStore::batchIsExist,
             py::call_guard<py::gil_scoped_release>(), py::arg("keys"),
             "Check if multiple objects exist. Returns list of results: 1 if "
             "exists, 0 if not exists, -1 if error")
        .def("close", &DistributedObjectStore::tearDownAll)
        .def("get_size", &DistributedObjectStore::getSize,
             py::call_guard<py::gil_scoped_release>())
        .def(
            "register_buffer",
            [](DistributedObjectStore &self, uintptr_t buffer_ptr,
               size_t size) {
                // Register memory buffer for RDMA operations
                void *buffer = reinterpret_cast<void *>(buffer_ptr);
                py::gil_scoped_release release;
                return self.register_buffer(buffer, size);
            },
            py::arg("buffer_ptr"), py::arg("size"),
            "Register a memory buffer for direct access operations")
        .def(
            "get_into",
            [](DistributedObjectStore &self, const std::string &key,
               uintptr_t buffer_ptr, size_t size) {
                // Get data directly into user-provided buffer
                void *buffer = reinterpret_cast<void *>(buffer_ptr);
                py::gil_scoped_release release;
                return self.get_into(key, buffer, size);
            },
            py::arg("key"), py::arg("buffer_ptr"), py::arg("size"),
            "Get object data directly into a pre-allocated buffer")
        .def(
            "batch_get_into",
            [](DistributedObjectStore &self,
               const std::vector<std::string> &keys,
               const std::vector<uintptr_t> &buffer_ptrs,
               const std::vector<size_t> &sizes) {
                std::vector<void *> buffers;
                buffers.reserve(buffer_ptrs.size());
                for (uintptr_t ptr : buffer_ptrs) {
                    buffers.push_back(reinterpret_cast<void *>(ptr));
                }
                py::gil_scoped_release release;
                return self.batch_get_into(keys, buffers, sizes);
            },
            py::arg("keys"), py::arg("buffer_ptrs"), py::arg("sizes"),
            "Get object data directly into pre-allocated buffers for multiple "
            "keys")
        .def(
            "put_from",
            [](DistributedObjectStore &self, const std::string &key,
               uintptr_t buffer_ptr, size_t size) {
                // Put data directly from user-provided buffer
                void *buffer = reinterpret_cast<void *>(buffer_ptr);
                py::gil_scoped_release release;
                return self.put_from(key, buffer, size);
            },
            py::arg("key"), py::arg("buffer_ptr"), py::arg("size"),
            "Put object data directly from a pre-allocated buffer")
        .def(
            "batch_put_from",
            [](DistributedObjectStore &self,
               const std::vector<std::string> &keys,
               const std::vector<uintptr_t> &buffer_ptrs,
               const std::vector<size_t> &sizes) {
                std::vector<void *> buffers;
                buffers.reserve(buffer_ptrs.size());
                for (uintptr_t ptr : buffer_ptrs) {
                    buffers.push_back(reinterpret_cast<void *>(ptr));
                }
                py::gil_scoped_release release;
                return self.batch_put_from(keys, buffers, sizes);
            },
            py::arg("keys"), py::arg("buffer_ptrs"), py::arg("sizes"),
            "Put object data directly from pre-allocated buffers for multiple "
            "keys")
        .def("put",
            [](DistributedObjectStore &self, const std::string &key, const py::buffer &buf) {
                py::buffer_info info = buf.request(/*writable=*/false);
                mmc_buffer buffer = {
                    .addr=reinterpret_cast<uint64_t>(info.ptr), \
                    .type=0,
                    .dram={.offset=0, .len=static_cast<uint64_t>(info.size)}
                };
                py::gil_scoped_release release;
                return self.put(key, buffer);
            })
        .def("put_parts",
            [](DistributedObjectStore &self, const std::string &key, const py::args& parts) {
                // 1) Python buffer → mmc_buffer
                std::vector<py::buffer_info> infos;
                std::vector<mmc_buffer> buffers;
                infos.reserve(parts.size());
                buffers.reserve(parts.size());

                for (auto &obj : parts) {
                    py::buffer buf = py::reinterpret_borrow<py::buffer>(obj);
                    infos.emplace_back(buf.request(false));
                    const auto &info = infos.back();
                    if (info.ndim != 1 || info.itemsize != 1)
                        throw std::runtime_error("parts must be 1-D bytes-like");

                    buffers.emplace_back(mmc_buffer{
                        .addr=reinterpret_cast<uint64_t>(info.ptr),
                        .type=0,
                        .dram={.offset=0, .len=static_cast<uint64_t>(info.size)}});
                }

                // 2) Call C++ function
                py::gil_scoped_release unlock;
                return self.put_parts(key, buffers);
            })
        .def(
            "put_batch",
            [](DistributedObjectStore &self, const std::vector<std::string> &keys, const std::vector<py::bytes> &py_values) {
                std::vector<std::string> temp_values;
                temp_values.reserve(py_values.size());
                for (const auto &value : py_values) {
                    temp_values.emplace_back(value.cast<std::string>());
                }

                std::vector<mmc_buffer> buffers;
                buffers.reserve(temp_values.size());
                for (const auto &s : temp_values) {
                    buffers.emplace_back(mmc_buffer{
                        .addr=reinterpret_cast<uintptr_t>(s.data()),
                        .type=0,
                        .dram={.offset=0, .len=s.size()}});
                }

                return self.put_batch(keys, buffers);
            },
            py::arg("keys"), py::arg("values"));
}
