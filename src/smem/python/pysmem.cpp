/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */
#include <Python.h>
#include <pybind11/pybind11.h>
#include <pybind11/pytypes.h>

#include "smem_shm.h"
#include "smem_bm.h"

namespace py = pybind11;

namespace {
class ShareMemory {
public:
    explicit ShareMemory(smem_shm_t hd, void *gva) noexcept : handle_{hd}, gvaAddress_{gva} {}
    virtual ~ShareMemory() noexcept
    {
        smem_shm_destroy(handle_, 0);
    }

    void SetExternContext(const void *context, uint32_t size)
    {
        auto ret = smem_shm_set_extra_context(handle_, context, size);
        if (ret != 0) {
            throw std::runtime_error("set extern context failed:");
        }
    }

    uint32_t LocalRank() noexcept
    {
        return smem_shm_get_global_rank(handle_);
    }

    uint32_t RankSize() noexcept
    {
        return smem_shm_get_global_rank_size(handle_);
    }

    void Barrier()
    {
        auto ret = smem_shm_control_barrier(handle_);
        if (ret != 0) {
            throw std::runtime_error("barrier failed:");
        }
    }

    void AllGather(const char *sendBuf, uint32_t sendSize, char *recvBuf, uint32_t recvSize)
    {
        auto ret = smem_shm_control_allgather(handle_, sendBuf, sendSize, recvBuf, recvSize);
        if (ret != 0) {
            throw std::runtime_error("all gather failed:");
        }
    }

    void *Address() const noexcept
    {
        return gvaAddress_;
    }

    static int Initialize(const std::string &storeURL, uint32_t worldSize, uint32_t rankId, uint16_t deviceId,
                          smem_shm_config_t &config) noexcept
    {
        return smem_shm_init(storeURL.c_str(), worldSize, rankId, deviceId, &config);
    }

    static void UnInitialize(uint32_t flags) noexcept
    {
        smem_shm_uninit(flags);
    }

    static ShareMemory *Create(uint32_t id, uint32_t rankSize, uint32_t rankId, uint64_t symmetricSize,
                               smem_shm_data_op_type dataOpType, uint32_t flags)
    {
        void *gva;
        auto handle = smem_shm_create(id, rankSize, rankId, symmetricSize, dataOpType, flags, &gva);
        if (handle == nullptr) {
            throw std::runtime_error("create shm failed!");
        }

        return new ShareMemory(handle, gva);
    }

private:
    smem_shm_t handle_;
    void *gvaAddress_;
};

class BigMemory {
public:
    BigMemory(smem_bm_t hd) noexcept : handle_{hd} {}
    virtual ~BigMemory() noexcept
    {
        smem_bm_destroy(handle_);
    }

    uint64_t Join(uint32_t flags)
    {
        void *address;
        auto ret = smem_bm_join(handle_, flags, &address);
        if (ret != 0) {
            throw std::runtime_error(std::string("join bm failed:").append(std::to_string(ret)));
        }
        return (uint64_t)(ptrdiff_t)address;
    }

    void Leave(uint32_t flags)
    {
        auto ret = smem_bm_leave(handle_, flags);
        if (ret != 0) {
            throw std::runtime_error(std::string("leave bm failed:").append(std::to_string(ret)));
        }
    }

    uint64_t LocalMemSize()
    {
        return smem_bm_get_local_mem_size(handle_);
    }

    uint64_t GetPtrByRank(uint32_t rankId)
    {
        auto ptr = smem_bm_ptr(handle_, rankId);
        if (ptr == nullptr) {
            throw std::runtime_error(std::string("get remote ptr failed:"));
        }

        return (uint64_t)(ptrdiff_t)ptr;
    }

    void CopyData(uint64_t src, uint64_t dest, uint64_t size, smem_bm_copy_type type, uint32_t flags)
    {
        auto ret = smem_bm_copy(handle_, (const void *)(ptrdiff_t)src, (void *)(ptrdiff_t)dest, size, type, flags);
        if (ret != 0) {
            throw std::runtime_error(std::string("copy bm data failed:").append(std::to_string(ret)));
        }
    }

    static int Initialize(const std::string &storeURL, uint32_t worldSize, uint16_t deviceId,
                          const smem_bm_config_t &config) noexcept
    {
        worldSize_ = worldSize;
        return smem_bm_init(storeURL.c_str(), worldSize, deviceId, &config);
    }

    static void UnInitialize(uint32_t flags) noexcept
    {
        smem_bm_uninit(flags);
    }

    static uint32_t GetRankId() noexcept
    {
        return smem_bm_get_rank_id();
    }

    static BigMemory *Create(uint32_t id, uint64_t localMemorySize, smem_bm_mem_type memType,
                             smem_bm_data_op_type dataOpType, uint32_t flags)
    {
        auto hd = smem_bm_create(id, worldSize_, memType, dataOpType, localMemorySize, flags);
        if (hd == nullptr) {
            throw std::runtime_error(std::string("create bm handle failed."));
        }

        return new BigMemory{hd};
    }

private:
    smem_bm_t handle_;
    static uint32_t worldSize_;
};

uint32_t BigMemory::worldSize_;

void DefineShmConfig(py::module_ &m)
{
    py::class_<smem_shm_config_t>(m, "ShmConfig")
        .def(py::init([]() {
            auto config = new smem_shm_config_t;
            smem_shm_config_init(config);
            return config;
        }))
        .def_readwrite("init_timeout", &smem_shm_config_t::shmInitTimeout)
        .def_readwrite("create_timeout", &smem_shm_config_t::shmCreateTimeout)
        .def_readwrite("operation_timeout", &smem_shm_config_t::controlOperationTimeout)
        .def_readwrite("start_store", &smem_shm_config_t::startConfigStore)
        .def_readwrite("flags", &smem_shm_config_t::flags);
}

void DefineBmConfig(py::module_ &m)
{
    py::enum_<smem_bm_copy_type>(m, "BmCopyType")
        .value("L2G", SMEMB_COPY_L2G)
        .value("G2L", SMEMB_COPY_G2L)
        .value("G2H", SMEMB_COPY_G2H)
        .value("H2G", SMEMB_COPY_H2G);

    py::class_<smem_bm_config_t>(m, "BmConfig")
        .def(py::init([]() {
            auto config = new smem_bm_config_t;
            smem_bm_config_init(config);
            return config;
        }))
        .def_readwrite("init_timeout", &smem_bm_config_t::initTimeout)
        .def_readwrite("create_timeout", &smem_bm_config_t::createTimeout)
        .def_readwrite("operation_timeout", &smem_bm_config_t::controlOperationTimeout)
        .def_readwrite("start_store", &smem_bm_config_t::startConfigStore)
        .def_readwrite("start_store_only", &smem_bm_config_t::startConfigStoreOnly)
        .def_readwrite("dynamic_world_size", &smem_bm_config_t::dynamicWorldSize)
        .def_readwrite("unified_address_space", &smem_bm_config_t::unifiedAddressSpace)
        .def_readwrite("auto_ranking", &smem_bm_config_t::autoRanking)
        .def_readwrite("rank_id", &smem_bm_config_t::rankId)
        .def_readwrite("flags", &smem_bm_config_t::flags);
}

void DefineShmClass(py::module_ &m)
{
    py::enum_<smem_shm_data_op_type>(m, "ShmDataOpType")
        .value("MTE", SMEMS_DATA_OP_MTE)
        .value("SDMA", SMEMS_DATA_OP_SDMA)
        .value("ROCE", SMEMS_DATA_OP_ROCE);

    py::class_<ShareMemory>(m, "ShareMemory")
        .def_static("initialize", &ShareMemory::Initialize, py::call_guard<py::gil_scoped_release>(),
                    py::arg("store_url"), py::arg("world_size"), py::arg("rank_id"), py::arg("device_id"),
                    py::arg("config"))
        .def_static("uninitialize", &ShareMemory::UnInitialize, py::call_guard<py::gil_scoped_release>(),
                    py::arg("flags") = 0)
        .def_static("create", &ShareMemory::Create, py::call_guard<py::gil_scoped_release>(), py::arg("id"),
                    py::arg("rank_size"), py::arg("rank_id"), py::arg("local_mem_size"),
                    py::arg("data_op_type") = SMEMS_DATA_OP_MTE, py::arg("flags") = 0)
        .def(
            "set_context",
            [](ShareMemory &shm, py::bytes data) {
                auto str = py::bytes(data).cast<std::string>();
                shm.SetExternContext(str.data(), str.size());
            },
            py::call_guard<py::gil_scoped_release>(), py::arg("context"))
        .def_property_readonly("local_rank", &ShareMemory::LocalRank, py::call_guard<py::gil_scoped_release>())
        .def_property_readonly("rank_size", &ShareMemory::RankSize, py::call_guard<py::gil_scoped_release>())
        .def("barrier", &ShareMemory::Barrier, py::call_guard<py::gil_scoped_release>())
        .def(
            "all_gather",
            [](ShareMemory &shm, py::bytes data) {
                auto str = py::bytes(data).cast<std::string>();
                auto outputSize = str.size() * shm.RankSize();
                std::string output;
                output.resize(outputSize);
                shm.AllGather(str.c_str(), str.size(), output.data(), outputSize);
                return py::bytes(output.c_str(), outputSize);
            },
            py::call_guard<py::gil_scoped_release>(), py::arg("local_data"))
        .def_property_readonly(
            "gva", [](const ShareMemory &shm) { return (uint64_t)(ptrdiff_t)shm.Address(); },
            py::call_guard<py::gil_scoped_release>());
}

void DefineBmClass(py::module_ &m)
{
    py::enum_<smem_bm_mem_type>(m, "BmMemType").value("HBM", SMEMB_MEM_TYPE_HBM).value("DRAM", SMEMB_MEM_TYPE_DRAM);
    py::enum_<smem_bm_data_op_type>(m, "BmDataOpType")
        .value("SDMA", SMEMB_DATA_OP_SDMA)
        .value("ROCE", SMEMB_DATA_OP_ROCE);

    py::class_<BigMemory>(m, "BigMemory")
        .def_static("initialize", &BigMemory::Initialize, py::call_guard<py::gil_scoped_release>(),
                    py::arg("store_url"), py::arg("world_size"), py::arg("device_id"), py::arg("config"))
        .def_static("uninitialize", &BigMemory::UnInitialize, py::call_guard<py::gil_scoped_release>(),
                    py::arg("flags") = 0)
        .def_static("bm_rank_id", &BigMemory::GetRankId, py::call_guard<py::gil_scoped_release>())
        .def_static("create", &BigMemory::Create, py::call_guard<py::gil_scoped_release>(), py::arg("id"),
                    py::arg("local_mem_size"), py::arg("mem_type") = SMEMB_MEM_TYPE_HBM,
                    py::arg("data_op_type") = SMEMB_DATA_OP_SDMA, py::arg("flags") = 0)
        .def("join", &BigMemory::Join, py::call_guard<py::gil_scoped_release>(), py::arg("flags") = 0)
        .def("leave", &BigMemory::Leave, py::call_guard<py::gil_scoped_release>(), py::arg("flags") = 0)
        .def("local_mem_size", &BigMemory::LocalMemSize, py::call_guard<py::gil_scoped_release>())
        .def("peer_rank_ptr", &BigMemory::GetPtrByRank, py::call_guard<py::gil_scoped_release>(), py::arg("peer_rank"))
        .def("copy_data", &BigMemory::CopyData, py::call_guard<py::gil_scoped_release>(), py::arg("src_ptr"),
             py::arg("dst_ptr"), py::arg("size"), py::arg("type"), py::arg("flags") = 0);
}
}

PYBIND11_MODULE(smem, m)
{
    DefineShmConfig(m);
    DefineBmConfig(m);
    DefineShmClass(m);
    DefineBmClass(m);
}