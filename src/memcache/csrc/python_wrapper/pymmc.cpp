/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */

#include <algorithm>
#include "mmc_client.h"
#include "mmc.h"
#include "mmc_ptracer.h"
#include "mmc_meta_service_process.h"
#include "smem_bm_def.h"
#include "pymmc.h"

namespace py = pybind11;
using namespace ock::mmc;
using namespace ock::mf;

void DefineMmcStructModule(py::module_ &m)
{
    py::enum_<smem_bm_copy_type>(m, "MmcCopyDirect")
        .value("SMEMB_COPY_L2G", SMEMB_COPY_L2G)
        .value("SMEMB_COPY_G2L", SMEMB_COPY_G2L)
        .value("SMEMB_COPY_G2H", SMEMB_COPY_G2H)
        .value("SMEMB_COPY_H2G", SMEMB_COPY_H2G);

    // Define the KeyInfo class
    py::class_<KeyInfo, std::shared_ptr<KeyInfo>>(m, "KeyInfo", py::buffer_protocol())
        .def("size", &KeyInfo::Size)
        .def("loc_list", &KeyInfo::GetLocs)
        .def("type_list", &KeyInfo::GetTypes)
        .def("__str__", &KeyInfo::ToString)
        .def("__repr__", &KeyInfo::ToString);

    py::class_<ReplicateConfig>(m, "ReplicateConfig", R"pbdoc(
         Configuration for replica allocation policy.
     )pbdoc")
        .def(py::init<>(), R"pbdoc(
         Default constructor.
         Initializes:
           - preferredLocalServiceIDs = {}
           - replicaNum = 0
     )pbdoc")
        .def_readwrite("replicaNum", &ReplicateConfig::replicaNum, R"pbdoc(
         Less than or equal to 8.
     )pbdoc")
        .def_readwrite("preferredLocalServiceIDs", &ReplicateConfig::preferredLocalServiceIDs, R"pbdoc(
         List of instance IDs for forced storage. The values in the list must be unique,
         and the list size must be equal to replicaNum.
     )pbdoc");
}

PYBIND11_MODULE(_pymmc, m)
{
    DefineMmcStructModule(m);
    ock::mmc::ReplicateConfig defaultConfig;
    // Support starting the meta service from python
    py::class_<MmcMetaServiceProcess>(m, "MetaService")
        .def_static(
            "main", []() { return MmcMetaServiceProcess::getInstance().MainForPython(); },
            "Start the meta service process directly. This is a blocking call.");

    // Define the MmcacheStore class
    py::class_<MmcacheStore>(m, "DistributedObjectStore")
        .def(py::init<>())
        .def("init", &MmcacheStore::Init, py::call_guard<py::gil_scoped_release>(), py::arg("device_id"))
        .def("remove", &MmcacheStore::Remove, py::call_guard<py::gil_scoped_release>())
        .def("remove_batch", &MmcacheStore::BatchRemove, py::call_guard<py::gil_scoped_release>())
        .def("is_exist", &MmcacheStore::IsExist, py::call_guard<py::gil_scoped_release>())
        .def("batch_is_exist", &MmcacheStore::BatchIsExist, py::call_guard<py::gil_scoped_release>(), py::arg("keys"),
             "Check if multiple objects exist. Returns list of results: 1 if exists, 0 if not exists, -1 if error")
        .def("get_key_info", &MmcacheStore::GetKeyInfo, py::call_guard<py::gil_scoped_release>())
        .def("batch_get_key_info", &MmcacheStore::BatchGetKeyInfo, py::call_guard<py::gil_scoped_release>(),
             py::arg("keys"))
        .def("close", &MmcacheStore::TearDown)
        .def(
            "register_buffer",
            [](MmcacheStore &self, uintptr_t buffer_ptr, size_t size) {
                // Register memory buffer for RDMA operations
                void *buffer = reinterpret_cast<void *>(buffer_ptr);
                py::gil_scoped_release release;
                return self.RegisterBuffer(buffer, size);
            },
            py::arg("buffer_ptr"), py::arg("size"), "Register a memory buffer for direct access operations")
        .def(
            "get_into",
            [](MmcacheStore &self, const std::string &key, uintptr_t buffer_ptr, size_t size, const int32_t &direct) {
                py::gil_scoped_release release;
                return self.GetInto(key, reinterpret_cast<void *>(buffer_ptr), size, direct);
            },
            py::arg("key"), py::arg("buffer_ptr"), py::arg("size"), py::arg("direct") = SMEMB_COPY_G2H,
            "Get object data directly into a pre-allocated buffer")
        .def(
            "batch_get_into",
            [](MmcacheStore &self, const std::vector<std::string> &keys, const std::vector<uintptr_t> &buffer_ptrs,
               const std::vector<size_t> &sizes, const int32_t &direct) {
                std::vector<void *> buffers;
                buffers.reserve(buffer_ptrs.size());
                for (uintptr_t ptr : buffer_ptrs) {
                    buffers.push_back(reinterpret_cast<void *>(ptr));
                }
                py::gil_scoped_release release;
                return self.BatchGetInto(keys, buffers, sizes, direct);
            },
            py::arg("keys"), py::arg("buffer_ptrs"), py::arg("sizes"), py::arg("direct") = SMEMB_COPY_G2H,
            "Get object data directly into pre-allocated buffers for multiple "
            "keys")
        .def(
            "get_into_layers",
            [](MmcacheStore &self, const std::string &key, const std::vector<uintptr_t> &buffer_ptrs,
               const std::vector<size_t> &sizes, const int32_t &direct) {
                TP_TRACE_BEGIN(TP_MMC_PYBIND_GET_LAYERS);
                std::vector<void *> buffers;
                buffers.reserve(buffer_ptrs.size());
                for (uintptr_t ptr : buffer_ptrs) {
                    buffers.push_back(reinterpret_cast<void *>(ptr));
                }
                py::gil_scoped_release release;
                auto ret = self.GetIntoLayers(key, buffers, sizes, direct);
                TP_TRACE_END(TP_MMC_PYBIND_GET_LAYERS, 0);
                return ret;
            },
            py::arg("key"), py::arg("buffer_ptrs"), py::arg("sizes"), py::arg("direct") = SMEMB_COPY_G2H)
        .def(
            "batch_get_into_layers",
            [](MmcacheStore &self, const std::vector<std::string> &keys,
               const std::vector<std::vector<uintptr_t>> &buffer_ptrs, const std::vector<std::vector<size_t>> &sizes,
               const int32_t &direct) {
                TP_TRACE_BEGIN(TP_MMC_PYBIND_BATCH_GET_LAYERS);
                std::vector<std::vector<void *>> buffers;
                buffers.reserve(buffer_ptrs.size());
                for (auto vec : buffer_ptrs) {
                    std::vector<void *> tmp;
                    for (uintptr_t ptr : vec) {
                        tmp.push_back(reinterpret_cast<void *>(ptr));
                    }
                    buffers.push_back(tmp);
                }
                py::gil_scoped_release release;
                auto ret = self.BatchGetIntoLayers(keys, buffers, sizes, direct);
                TP_TRACE_END(TP_MMC_PYBIND_BATCH_GET_LAYERS, 0);
                return ret;
            },
            py::arg("keys"), py::arg("buffer_ptrs"), py::arg("sizes"), py::arg("direct") = SMEMB_COPY_G2H)
        .def(
            "get_local_service_id",
            [](MmcacheStore &self) {
                py::gil_scoped_release release;
                uint32_t localServiceId = std::numeric_limits<uint32_t>::max();
                self.GetLocalServiceId(localServiceId);
                return localServiceId;
            },
            "Get local serviceId")
        .def(
            "put_from",
            [](MmcacheStore &self, const std::string &key, uintptr_t buffer_ptr, size_t size, const int32_t &direct,
               const ReplicateConfig &replicateConfig) {
                py::gil_scoped_release release;
                return self.PutFrom(key, reinterpret_cast<void *>(buffer_ptr), size, direct, replicateConfig);
            },
            py::arg("key"), py::arg("buffer_ptr"), py::arg("size"), py::arg("direct") = SMEMB_COPY_H2G,
            py::arg("replicateConfig") = defaultConfig, "Put object data directly from a pre-allocated buffer")
        .def(
            "batch_put_from",
            [](MmcacheStore &self, const std::vector<std::string> &keys, const std::vector<uintptr_t> &buffer_ptrs,
               const std::vector<size_t> &sizes, const int32_t &direct, const ReplicateConfig &replicateConfig) {
                std::vector<void *> buffers;
                buffers.reserve(buffer_ptrs.size());
                for (uintptr_t ptr : buffer_ptrs) {
                    buffers.push_back(reinterpret_cast<void *>(ptr));
                }
                py::gil_scoped_release release;
                return self.BatchPutFrom(keys, buffers, sizes, direct, replicateConfig);
            },
            py::arg("keys"), py::arg("buffer_ptrs"), py::arg("sizes"), py::arg("direct") = SMEMB_COPY_H2G,
            py::arg("replicateConfig") = defaultConfig,
            "Put object data directly from pre-allocated buffers for multiple "
            "keys")
        .def(
            "put_from_layers",
            [](MmcacheStore &self, const std::string &key, const std::vector<uintptr_t> &buffer_ptrs,
               const std::vector<size_t> &sizes, const int32_t &direct, const ReplicateConfig &replicateConfig) {
                TP_TRACE_BEGIN(TP_MMC_PYBIND_PUT_LAYERS);
                std::vector<void *> buffers;
                buffers.reserve(buffer_ptrs.size());
                for (uintptr_t ptr : buffer_ptrs) {
                    buffers.push_back(reinterpret_cast<void *>(ptr));
                }
                py::gil_scoped_release release;
                auto ret = self.PutFromLayers(key, buffers, sizes, direct, replicateConfig);
                TP_TRACE_END(TP_MMC_PYBIND_PUT_LAYERS, 0);
                return ret;
            },
            py::arg("key"), py::arg("buffer_ptrs"), py::arg("sizes"), py::arg("direct") = SMEMB_COPY_H2G,
            py::arg("replicateConfig") = defaultConfig)
        .def(
            "batch_put_from_layers",
            [](MmcacheStore &self, const std::vector<std::string> &keys,
               const std::vector<std::vector<uintptr_t>> &buffer_ptrs, const std::vector<std::vector<size_t>> &sizes,
               const int32_t &direct, const ReplicateConfig &replicateConfig) {
                TP_TRACE_BEGIN(TP_MMC_PYBIND_BATCH_PUT_LAYERS);
                std::vector<std::vector<void *>> buffers;
                buffers.reserve(buffer_ptrs.size());
                for (auto vec : buffer_ptrs) {
                    std::vector<void *> tmp;
                    for (uintptr_t ptr : vec) {
                        tmp.push_back(reinterpret_cast<void *>(ptr));
                    }
                    buffers.push_back(tmp);
                }
                py::gil_scoped_release release;
                auto ret = self.BatchPutFromLayers(keys, buffers, sizes, direct, replicateConfig);
                TP_TRACE_END(TP_MMC_PYBIND_BATCH_PUT_LAYERS, 0);
                return ret;
            },
            py::arg("keys"), py::arg("buffer_ptrs"), py::arg("sizes"), py::arg("direct") = SMEMB_COPY_H2G,
            py::arg("replicateConfig") = defaultConfig)
        .def("put",
            [](MmcacheStore &self, const std::string &key, const py::buffer &buf,
               const ReplicateConfig &replicateConfig) {
                py::buffer_info info = buf.request(false);
                mmc_buffer buffer = {.addr = reinterpret_cast<uint64_t>(info.ptr),
                                     .type = MEDIA_DRAM,
                                     .offset = 0,
                                     .len = static_cast<uint64_t>(info.size)};
                py::gil_scoped_release release;
                return self.Put(key, buffer, replicateConfig);
            },
            py::arg("key"), py::arg("buf"), py::arg("replicateConfig") = defaultConfig)
        .def("get", [](MmcacheStore &self, const std::string &key) {
            mmc_buffer buffer = self.Get(key);
            py::gil_scoped_acquire acquire_gil;
            if (buffer.addr == 0) {
                return py::bytes("");
            } else {
                auto dataPtr = reinterpret_cast<char *>(buffer.addr);
                std::shared_ptr<char[]> dataSharedPtr(dataPtr, [](char *p) { delete[] p; });
                return py::bytes(dataPtr, buffer.len);
            }
        });
}