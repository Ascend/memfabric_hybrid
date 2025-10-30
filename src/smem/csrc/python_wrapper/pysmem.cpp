/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */
#include <Python.h>
#include <pybind11/pybind11.h>
#include <pybind11/pytypes.h>
#include <cstdint>
#include <mutex>
#include <new>
#include "smem.h"
#include "smem_bm.h"
#include "smem_version.h"

namespace py = pybind11;

namespace {
class BigMemory {
public:
    explicit BigMemory(smem_bm_t hd) noexcept : handle_{hd} {}
    virtual ~BigMemory() noexcept
    {
        smem_bm_destroy(handle_);
    }

    void Join(uint32_t flags)
    {
        auto ret = smem_bm_join(handle_, flags);
        if (ret != 0) {
            throw std::runtime_error(std::string("join bm failed:").append(std::to_string(ret)));
        }
    }

    void Leave(uint32_t flags)
    {
        auto ret = smem_bm_leave(handle_, flags);
        if (ret != 0) {
            throw std::runtime_error(std::string("leave bm failed:").append(std::to_string(ret)));
        }
    }

    uint64_t LocalMemSize(smem_bm_mem_type memType)
    {
        return smem_bm_get_local_mem_size_by_mem_type(handle_, memType);
    }

    uint64_t GetPtrByRank(uint32_t rankId, smem_bm_mem_type memType)
    {
        auto ptr = smem_bm_ptr_by_mem_type(handle_, memType, rankId);
        if (ptr == nullptr) {
            throw std::runtime_error(std::string("get remote ptr failed:"));
        }

        return (uint64_t)(ptrdiff_t)ptr;
    }

    void Destroy()
    {
        smem_bm_destroy(handle_);
        handle_ = nullptr;
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

    static BigMemory *Create(uint32_t id, uint64_t localDRAMSize, uint64_t localHBMSize,
                             smem_bm_data_op_type dataOpType, uint32_t flags)
    {
        auto hd = smem_bm_create(id, worldSize_, dataOpType, localDRAMSize, localHBMSize, flags);
        if (hd == nullptr) {
            throw std::runtime_error(std::string("create bm handle failed."));
        }

        return new (std::nothrow) BigMemory{hd};
    }

    int RegisterMem(uint64_t addr, uint64_t size) noexcept
    {
        return smem_bm_register_user_mem(handle_, addr, size);
    }

    uint64_t RegisterHostMem(uint64_t addr, uint64_t size)
    {
        uint64_t target = 0;
        auto ret = smem_bm_register_host_mem(addr, size, &target);
        if (ret != 0) {
            throw std::runtime_error(std::string("register host memory failed: ").append(std::to_string(ret)));
        }
        return target;
    }

    void UnregisterHostMem(uint64_t addr)
    {
        auto ret = smem_bm_unregister_host_mem(addr);
        if (ret != 0) {
            throw std::runtime_error(std::string("unregister host memory failed: ").append(std::to_string(ret)));
        }
    }

private:
    smem_bm_t handle_;
    static uint32_t worldSize_;
};

uint32_t BigMemory::worldSize_;
struct LoggerState {
    static std::mutex mutex;
    static std::shared_ptr<py::function> py_logger;
};

std::mutex LoggerState::mutex;
std::shared_ptr<py::function> LoggerState::py_logger;

static void cpp_logger_adapter(int level, const char *msg)
{
    std::lock_guard<std::mutex> lock(LoggerState::mutex);

    if (!LoggerState::py_logger) {
        return;
    }

    py::gil_scoped_acquire acquire;
    if (Py_IsInitialized()) {
        (*(LoggerState::py_logger))(level, msg ? msg : "");
    }
}

void DefineSmemFunctions(py::module_ &m)
{
    m.def("initialize", &smem_init, py::call_guard<py::gil_scoped_release>(), py::arg("flags") = 0, R"(
Initialize the smem running environment.

Arguments:
    flags(int): optional flags, reserved
Returns:
    0 if successful
)");

    m.def("uninitialize", &smem_uninit, py::call_guard<py::gil_scoped_release>(), R"(
Un-Initialize the smem running environment)");

    m.def("set_log_level", &smem_set_log_level, py::call_guard<py::gil_scoped_release>(), py::arg("level"), R"(
set log print level.

Arguments:
    level(int): log level, 0:debug 1:info 2:warn 3:error)");
    m.def(
        "set_extern_logger",
        [](py::function log_fn) {
            if (!log_fn || log_fn.is_none()) {
                std::lock_guard<std::mutex> lock(LoggerState::mutex);
                LoggerState::py_logger.reset();
                auto ret = smem_set_extern_logger(nullptr);
                return ret;
            }

            {
                std::lock_guard<std::mutex> lock(LoggerState::mutex);
                LoggerState::py_logger = std::make_shared<py::function>(log_fn);
            }

            auto ret = smem_set_extern_logger(cpp_logger_adapter);
            if (ret != 0) {
                throw std::runtime_error("Failed to set logger");
            }
            return ret;
        },
        py::call_guard<py::gil_scoped_release>(), py::arg("log_fn"), R"(
Set external logger callback function

Parameters:
    log_fn (callable): Python function that accepts (int level, str message)
        level: log level
        message: log content
Returns:
    0 if successful
)");

    m.add_object("_cleanup_capsule", py::capsule([]() { LoggerState::py_logger.reset(); }));

    m.def("get_last_err_msg", &smem_get_last_err_msg, py::call_guard<py::gil_scoped_release>(), R"(
Get last error message.
Returns:
    error message string
)");

    m.def("get_and_clear_last_err_msg", &smem_get_and_clear_last_err_msg, py::call_guard<py::gil_scoped_release>(), R"(
Get and clear all error message.
Returns:
    error message string
)");
    m.doc() = LIB_VERSION;
}

// enum wrap for binding
enum smem_bm_init_flag : uint32_t {
    SMEM_BM_INIT_FLAG_GVM = SMEM_BM_INIT_GVM_FLAG,
};

void DefineBmConfig(py::module_ &m)
{
    py::enum_<smem_bm_mem_type>(m, "BmMemType")
        .value("DEVICE", SMEM_MEM_TYPE_DEVICE, "memory type is DEVICE side.")
        .value("HOST", SMEM_MEM_TYPE_HOST, "memory type is HOST side.");

    py::enum_<smem_bm_copy_type>(m, "BmCopyType")
        .value("L2G", SMEMB_COPY_L2G, "copy data from local space to global space")
        .value("G2L", SMEMB_COPY_G2L, "copy data from global space to local space")
        .value("G2H", SMEMB_COPY_G2H, "copy data from global space to host memory")
        .value("H2G", SMEMB_COPY_H2G, "copy data from host memory to global space")
        .value("G2G", SMEMB_COPY_G2G, "copy data from global space to global space");

    py::enum_<smem_bm_init_flag>(m, "BmInitFlag").value("GVM", SMEM_BM_INIT_FLAG_GVM, "enable GVM");

    py::class_<smem_bm_config_t>(m, "BmConfig")
        .def(py::init([]() {
                 auto config = new (std::nothrow) smem_bm_config_t;
                 smem_bm_config_init(config);
                 return config;
             }),
             py::call_guard<py::gil_scoped_release>())
        .def_readwrite("init_timeout", &smem_bm_config_t::initTimeout, R"(
func smem_bm_init timeout, default 120 second)")
        .def_readwrite("create_timeout", &smem_bm_config_t::createTimeout, R"(
func smem_bm_create timeout, default 120 second)")
        .def_readwrite("operation_timeout", &smem_bm_config_t::controlOperationTimeout, R"(
control operation timeout, default 120 second)")
        .def_readwrite("start_store", &smem_bm_config_t::startConfigStoreServer, R"(
whether to start config store, default true)")
        .def_readwrite("start_store_only", &smem_bm_config_t::startConfigStoreOnly, "only start the config store")
        .def_readwrite("dynamic_world_size", &smem_bm_config_t::dynamicWorldSize, "member cannot join dynamically")
        .def_readwrite("unified_address_space", &smem_bm_config_t::unifiedAddressSpace, "unified address with SVM")
        .def_readwrite("auto_ranking", &smem_bm_config_t::autoRanking, R"(
automatically allocate rank IDs, default is false)")
        .def_readwrite("rank_id", &smem_bm_config_t::rankId, "user specified rank ID, valid for autoRanking is False")
        .def_readwrite("flags", &smem_bm_config_t::flags, "other flags, default 0")
        .def(
            "set_nic",
            [](smem_bm_config_t &config, const std::string &nic) {
                strncpy(config.hcomUrl, nic.c_str(), sizeof(config.hcomUrl));
            },
            py::call_guard<py::gil_scoped_release>(), py::arg("nic"));
}

void DefineBmClass(py::module_ &m)
{
    py::enum_<smem_bm_data_op_type>(m, "BmDataOpType")
        .value("SDMA", SMEMB_DATA_OP_SDMA)
        .value("HOST_RDMA", SMEMB_DATA_OP_HOST_RDMA)
        .value("HOST_TCP", SMEMB_DATA_OP_HOST_TCP)
        .value("DEVICE_RDMA", SMEMB_DATA_OP_DEVICE_RDMA);

    // module method
    m.def("initialize", &BigMemory::Initialize, py::call_guard<py::gil_scoped_release>(), py::arg("store_url"),
          py::arg("world_size"), py::arg("device_id"), py::arg("config"), R"(
Initialize smem big memory library.

Arguments:
    store_url(str):   configure store url for control, e.g. tcp:://ip:port
    world_size(int):  number of guys participating
    device_id(int):   device id
    config(BmConfig): extract config
Returns:
    0 if successful)");

    m.def("uninitialize", &BigMemory::UnInitialize, py::call_guard<py::gil_scoped_release>(), py::arg("flags") = 0, R"(
Un-initialize the smem big memory library.

Arguments:
    flags(int): optional flags, not used yet)");

    m.def("bm_rank_id", &BigMemory::GetRankId, py::call_guard<py::gil_scoped_release>(), R"(
Get the rank id, assigned during initialize.
Returns:
    rank id if successful, UINT32_MAX is returned if failed.)");

    m.def("create", &BigMemory::Create, py::call_guard<py::gil_scoped_release>(), py::arg("id"),
          py::arg("local_dram_size"), py::arg("local_hbm_size"), py::arg("data_op_type") = SMEMB_DATA_OP_SDMA,
          py::arg("flags") = 0, R"(
Create a big memory object locally after initialized.

Arguments:
    id(int):                     identity of the big memory object
    local_dram_size(int):         the size of local dram memory contributes to big memory object
    local_hbm_size(int):         the size of local hbm memory contributes to big memory object
    data_op_type(BmDataOpType):  data operation type, SDMA or RoCE etc
    flags(int):                  optional flags)");

    // big memory class
    py::class_<BigMemory>(m, "BigMemory")
        .def("join", &BigMemory::Join, py::call_guard<py::gil_scoped_release>(), py::arg("flags") = 0, R"(
Join to global Big Memory space actively, after this, we operate on the global space.

Arguments:
    flags(int): optional flags)")
        .def("leave", &BigMemory::Leave, py::call_guard<py::gil_scoped_release>(), py::arg("flags") = 0, R"(
Leave the global Big Memory space actively, after this, we cannot operate on the global space any more.

Arguments:
    flags(int): optional flags)")
        .def("local_mem_size", &BigMemory::LocalMemSize, py::call_guard<py::gil_scoped_release>(),
             py::arg("mem_type") = SMEM_MEM_TYPE_DEVICE, R"(
Get size of local memory that contributed to global space.

Arguments:
    mem_type(BmMemType): memory type, DEVICE or HOST, default is DEVICE
Returns:
    local memory size in bytes)")
        .def("peer_rank_ptr", &BigMemory::GetPtrByRank, py::call_guard<py::gil_scoped_release>(), py::arg("peer_rank"),
             py::arg("mem_type") = SMEM_MEM_TYPE_DEVICE,
             R"(
Get peer gva by rank id.

Arguments:
    peer_rank(int): rank id of peer
    mem_type(BmMemType): memory type, DEVICE or HOST, default is DEVICE
Returns:
    ptr of peer gva)")
        .def("destroy", &BigMemory::Destroy, py::call_guard<py::gil_scoped_release>(), R"(
Destroy the big memory handle.)")
        .def("register", &BigMemory::RegisterMem, py::call_guard<py::gil_scoped_release>(), py::arg("addr"),
             py::arg("size"), R"(
register user mem.)")
        .def("register_host_mem", &BigMemory::RegisterHostMem, py::call_guard<py::gil_scoped_release>(),
             py::arg("addr"), py::arg("size"), R"(
register user mem.

Arguments:
    addr(int): Requested the src share memory pointer, addr must be page aligned.
    size(int): Requested byte size.
Returns:
    pointer that stores the address of the allocated dst memory pointer.)")
        .def("unregister_host_mem", &BigMemory::UnregisterHostMem, py::call_guard<py::gil_scoped_release>(),
             py::arg("addr"), R"(unregister user mem.)")
        .def("copy_data", &BigMemory::CopyData, py::call_guard<py::gil_scoped_release>(), py::arg("src_ptr"),
             py::arg("dst_ptr"), py::arg("size"), py::arg("type"), py::arg("flags") = 0, R"(
Data operation on Big Memory object.

Arguments:
    src_ptr(int): source gva of data
    dst_ptr(int): destination gva of data
    size(int): size of data to be copied
    type(BmCopyType): copy type, L2G, G2L, G2H, H2G
    flags(int): optional flags
Returns:
    0 if successful)");
}
} // namespace

PYBIND11_MODULE(_pymf_smem, m)
{
    DefineSmemFunctions(m);

    auto bm = m.def_submodule("bm", "Big Memory Module.");

    DefineBmConfig(bm);
    DefineBmClass(bm);
}