/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 1.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <iostream>
#include <Python.h>
#include <pybind11/pybind11.h>
#include <pybind11/pytypes.h>
#include <pybind11/stl.h>
#include <mutex>

#include "smem.h"
#include "smem_shm.h"
#include "smem_bm.h"
#include "smem_version.h"

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

    void Destroy(uint32_t flags)
    {
        auto ret = smem_shm_destroy(handle_, flags);
        if (ret != 0) {
            throw std::runtime_error("destroy failed:");
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

        return new (std::nothrow)ShareMemory(handle, gva);
    }

    uint32_t QuerySupportDataOp() noexcept
    {
        return smem_shm_query_support_data_operation();
    }

    uint32_t TopologyCanReach(uint32_t remoteRank, uint32_t *reachInfo)
    {
        return smem_shm_topology_can_reach(handle_, remoteRank, reachInfo);
    }

private:
    smem_shm_t handle_;
    void *gvaAddress_;
};

class BigMemory {
public:
    struct CopyData2DParams{
        uint64_t src;
        uint64_t spitch;
        uint64_t dest;
        uint64_t dpitch;
        uint64_t width;
        uint64_t height;
    };
public:
    explicit BigMemory(smem_bm_t hd) noexcept : handle_{hd} {}
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
        return static_cast<uint64_t>(reinterpret_cast<ptrdiff_t>(address));
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

    void Destroy()
    {
        smem_bm_destroy(handle_);
        handle_ = nullptr;
    }

    void CopyData(uint64_t src, uint64_t dest, uint64_t size, smem_bm_copy_type type, uint32_t flags)
    {
        smem_copy_params params = {(const void *)(ptrdiff_t)src, (void *)(ptrdiff_t)dest, size};
        auto ret = smem_bm_copy(handle_, &params, type, flags);
        if (ret != 0) {
            throw std::runtime_error(std::string("copy bm data failed:").append(std::to_string(ret)));
        }
    }

    void CopyData2D(CopyData2DParams &params, smem_bm_copy_type type, uint32_t flags)
    {
        smem_copy_2d_params copyParams = {(const void *)(ptrdiff_t)params.src, params.spitch,
                                          (void *)(ptrdiff_t)params.dest,
                                          params.dpitch, params.width, params.height};
        auto ret = smem_bm_copy_2d(handle_, &copyParams, type, flags);
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

        return new (std::nothrow)BigMemory{hd};
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

static void cpp_logger_adapter(int level, const char* msg) {
    std::lock_guard<std::mutex> lock(LoggerState::mutex);

    if (!LoggerState::py_logger) {
        return;
    }

    py::gil_scoped_acquire acquire;
    if (Py_IsInitialized()) {
        (*(LoggerState::py_logger))(level, msg ? msg : "");
    }
}

static py::function g_py_decrypt_func;
static constexpr size_t MAX_CIPHER_LEN = 10 * 1024 * 1024;

static int py_decrypt_handler_wrapper(const char *cipherText, size_t cipherTextLen, char *plainText,
                                      size_t &plainTextLen)
{
    if (cipherTextLen > MAX_CIPHER_LEN || !g_py_decrypt_func || g_py_decrypt_func.is_none()) {
        std::cerr << "input cipher len is too long or decrypt func invalid." << std::endl;
        return -1;
    }
    std::string plain;
    try {
        py::str py_cipher = py::str(cipherText, cipherTextLen);
        plain = py::cast<std::string>(g_py_decrypt_func(py_cipher).cast<py::str>());
        if (plain.size() >= plainTextLen) {
            std::cerr << "output cipher len is too long" << std::endl;
            std::fill(plain.begin(), plain.end(), 0);
            return -1;
        }

        std::copy(plain.begin(), plain.end(), plainText);
        plainText[plain.size()] = '\0';
        plainTextLen = plain.size();
        std::fill(plain.begin(), plain.end(), 0);
        return 0;
    } catch (const py::error_already_set &e) {
        return -1;
    }
}

int32_t smem_set_conf_store_tls_key(std::string &tls_pk, std::string &tls_pk_pw,
    py::function py_decrypt_func)
{
    if (!py_decrypt_func || py_decrypt_func.is_none()) {
        return smem_set_config_store_tls_key(tls_pk.c_str(), tls_pk.size(), tls_pk_pw.c_str(),
            tls_pk_pw.size(), nullptr);
    }

    g_py_decrypt_func = py_decrypt_func;
    return smem_set_config_store_tls_key(tls_pk.c_str(), tls_pk.size(), tls_pk_pw.c_str(),
        tls_pk_pw.size(), py_decrypt_handler_wrapper);
}


int32_t smem_set_conf_store_tls_adapt(bool enable, std::string &tls_info)
{
    return smem_set_conf_store_tls(enable, tls_info.c_str(), tls_info.size());
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

    m.add_object("_cleanup_capsule", py::capsule([]() {
        LoggerState::py_logger.reset();
    }));

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

    m.def("set_conf_store_tls_key", &smem_set_conf_store_tls_key,
          py::call_guard<py::gil_scoped_release>(), py::arg("tls_pk"), py::arg("tls_pk_pw"),
          py::arg("py_decrypt_func"), R"(
Register a Python decrypt handler.
Parameters:
    tls_pk (string): the content of tls private key string
    tls_pk_pw (string): the content of tls private key password string
    py_decrypt_func (callable): Python function that accepts (str cipher_text) and returns (str plain_text)
        cipher_text: the encrypted text (private key password)
        plain_text: the decrypted text (private key password)
Returns:
    None
)");

    m.def("set_conf_store_tls", &smem_set_conf_store_tls_adapt, py::call_guard<py::gil_scoped_release>(),
          py::arg("enable"), py::arg("tls_info"), R"(
set the config store tls info.
Parameters:
    enable (boolean): enable config store tls or not
        tls_info (string): tls config string
Returns:
    returns zero on success. On error, none-zero is returned.
)");

    m.doc() = LIB_VERSION;
}

void DefineShmConfig(py::module_ &m)
{
    py::class_<smem_shm_config_t>(m, "ShmConfig")
        .def(py::init([]() {
                 auto config = new (std::nothrow)smem_shm_config_t;
                 smem_shm_config_init(config);
                 return config;
             }),
             py::call_guard<py::gil_scoped_release>())
        .def_readwrite("init_timeout", &smem_shm_config_t::shmInitTimeout, R"(
func smem_shm_init timeout, default 120 second.)")
        .def_readwrite("create_timeout", &smem_shm_config_t::shmCreateTimeout, R"(
func smem_shm_create timeout, default 120 second)")
        .def_readwrite("operation_timeout", &smem_shm_config_t::controlOperationTimeout, R"(
control operation timeout, i.e. barrier, allgather, topology_can_reach etc, default 120 second)")
        .def_readwrite("start_store", &smem_shm_config_t::startConfigStore, R"(
whether to start config store, default true)")
        .def_readwrite("flags", &smem_shm_config_t::flags, "other flags, default 0");
}

void DefineBmCopyData2DParams(py::module_ &m)
{
    py::class_<BigMemory::CopyData2DParams>(m, "CopyData2DParams")
        .def(py::init<>())
        .def_readwrite("src", &BigMemory::CopyData2DParams::src, R"(
            source src of data.)")
        .def_readwrite("spitch", &BigMemory::CopyData2DParams::spitch, R"(
            source pitch of data.)")
        .def_readwrite("dest", &BigMemory::CopyData2DParams::dest, R"(
            destination src of data.)")
        .def_readwrite("dpitch", &BigMemory::CopyData2DParams::dpitch, R"(
            destination pitch of data.)")
        .def_readwrite("width", &BigMemory::CopyData2DParams::width, R"(
            width of data to be copied.)")
        .def_readwrite("height", &BigMemory::CopyData2DParams::height, R"(
            height of data to be copied.)");
}

void DefineBmConfig(py::module_ &m)
{
    py::enum_<smem_bm_copy_type>(m, "BmCopyType")
        .value("L2G", SMEMB_COPY_L2G, "copy data from local space to global space")
        .value("G2L", SMEMB_COPY_G2L, "copy data from global space to local space")
        .value("G2H", SMEMB_COPY_G2H, "copy data from global space to host memory")
        .value("H2G", SMEMB_COPY_H2G, "copy data from host memory to global space")
        .value("G2G", SMEMB_COPY_G2G, "copy data from global space to global space");

    py::class_<smem_bm_config_t>(m, "BmConfig")
        .def(py::init([]() {
                 auto config = new (std::nothrow)smem_bm_config_t;
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
        .def_readwrite("start_store", &smem_bm_config_t::startConfigStore, R"(
whether to start config store, default true)")
        .def_readwrite("start_store_only", &smem_bm_config_t::startConfigStoreOnly, "only start the config store")
        .def_readwrite("dynamic_world_size", &smem_bm_config_t::dynamicWorldSize, "member cannot join dynamically")
        .def_readwrite("unified_address_space", &smem_bm_config_t::unifiedAddressSpace, "unified address with SVM")
        .def_readwrite("auto_ranking", &smem_bm_config_t::autoRanking, R"(
automatically allocate rank IDs, default is false)")
        .def_readwrite("rank_id", &smem_bm_config_t::rankId, "user specified rank ID, valid for autoRanking is False")
        .def_readwrite("flags", &smem_bm_config_t::flags, "other flags, default 0")
        .def_property("hcom_url",
                      [](const smem_bm_config_t &self) {return std::string(self.hcomUrl);},
                      [](smem_bm_config_t &self, const std::string &value) {
                            std::copy_n(value.c_str(), sizeof(self.hcomUrl) - 1, self.hcomUrl);
                            self.hcomUrl[sizeof(self.hcomUrl) - 1] = '\0';
                        }, "hcom url info");
}

void DefineShmClass(py::module_ &m)
{
    py::enum_<smem_shm_data_op_type>(m, "ShmDataOpType")
        .value("MTE", SMEMS_DATA_OP_MTE)
        .value("SDMA", SMEMS_DATA_OP_SDMA)
        .value("RDMA", SMEMS_DATA_OP_RDMA);

    m.def("initialize", &ShareMemory::Initialize, py::call_guard<py::gil_scoped_release>(), py::arg("store_url"),
          py::arg("world_size"), py::arg("rank_id"), py::arg("device_id"), py::arg("config"));
    m.def("uninitialize", &ShareMemory::UnInitialize, py::call_guard<py::gil_scoped_release>(), py::arg("flags") = 0);
    m.def("create", &ShareMemory::Create, py::call_guard<py::gil_scoped_release>(), py::arg("id"), py::arg("rank_size"),
          py::arg("rank_id"), py::arg("local_mem_size"), py::arg("data_op_type") = SMEMS_DATA_OP_MTE,
          py::arg("flags") = 0);

    py::class_<ShareMemory>(m, "ShareMemory")
        .def(
            "set_context",
            [](ShareMemory &shm, py::bytes data) {
                auto str = py::bytes(data).cast<std::string>();
                shm.SetExternContext(str.data(), str.size());
            },
            py::call_guard<py::gil_scoped_release>(), py::arg("context"), R"(
Set user extra context of shm object.

Arguments:
    context(bytes): extra context
Returns:
    0 if successful)")
        .def_property_readonly("local_rank", &ShareMemory::LocalRank, py::call_guard<py::gil_scoped_release>(), R"(
Get local rank of a shm object)")
        .def_property_readonly("rank_size", &ShareMemory::RankSize, py::call_guard<py::gil_scoped_release>(), R"(
Get rank size of a shm object)")
        .def("destroy", &ShareMemory::Destroy, py::call_guard<py::gil_scoped_release>(), py::arg("flags") = 0, R"(
Destroy the shm handle.)")
        .def("query_support_data_operation", &ShareMemory::QuerySupportDataOp,
            py::call_guard<py::gil_scoped_release>(), R"(
Get supported data operations)")
        .def("barrier", &ShareMemory::Barrier, py::call_guard<py::gil_scoped_release>(), R"(
Do barrier on a shm object, using control network.)")
        .def(
            "all_gather",
            [](ShareMemory &shm, py::bytes data) {
                auto str = py::bytes(data).cast<std::string>();
                auto outputSize = str.size() * shm.RankSize();
                std::string output;
                output.resize(outputSize);
                shm.AllGather(str.c_str(), str.size(), const_cast<char*>(output.data()), outputSize);
                return py::bytes(output.c_str(), outputSize);
            },
            py::call_guard<py::gil_scoped_release>(), py::arg("local_data"), R"(
Do all gather on a shm object, using control network

Arguments:
    local_data(bytes): input data
Returns:
    output data)")
        .def("topology_can_reach",
            [](ShareMemory &shm, uint32_t remote_rank, uint32_t reach_info) {
                return shm.TopologyCanReach(remote_rank, &reach_info);
            }, py::call_guard<py::gil_scoped_release>(), py::arg("remote_rank"), py::arg("reach_info"), R"(
Query the topology reachability to a remote rank

Arguments:
    remote_rank (int): Target rank ID to check
    reach_info (int): Reachability information
Returns:
    int: 0 if successful)")
        .def_property_readonly(
            "gva", [](const ShareMemory &shm) { return (uint64_t)(ptrdiff_t)shm.Address(); },
            py::call_guard<py::gil_scoped_release>(), R"(
get global virtual address created, it can be passed to kernel to data operations)");
}

void DefineBmClass(py::module_ &m)
{
    py::enum_<smem_bm_data_op_type>(m, "BmDataOpType")
        .value("SDMA", SMEMB_DATA_OP_SDMA)
        .value("HOST_RDMA", SMEMB_DATA_OP_HOST_RDMA);

    // module method
    m.def("initialize", &BigMemory::Initialize, py::call_guard<py::gil_scoped_release>(), py::arg("store_url"),
          py::arg("world_size"), py::arg("device_id"), py::arg("config"), R"(
Initialize smem big memory library.

Arguments:
    store_url(str):   configure store url for control, e.g. tcp://ip:port or tcp6://[ip]:port
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
        .def("local_mem_size", &BigMemory::LocalMemSize, py::call_guard<py::gil_scoped_release>(), R"(
Get size of local memory that contributed to global space.

Returns:
    local memory size in bytes)")
        .def("peer_rank_ptr", &BigMemory::GetPtrByRank, py::call_guard<py::gil_scoped_release>(), py::arg("peer_rank"),
             R"(
Get peer gva by rank id.

Arguments:
    peer_rank(int): rank id of peer
Returns:
    ptr of peer gva)")
        .def("destroy", &BigMemory::Destroy, py::call_guard<py::gil_scoped_release>(), R"(
Destroy the big memory handle.)")
        .def("copy_data", &BigMemory::CopyData, py::call_guard<py::gil_scoped_release>(), py::arg("src_ptr"),
             py::arg("dst_ptr"), py::arg("size"), py::arg("type"), py::arg("flags") = 0, R"(
Data operation on Big Memory object.

Arguments:
    src_ptr(int): source gva of data
    dst_ptr(int): destination gva of data
    size(int): size of data to be copied
    type(BmCopyType): copy type, L2G, G2L, G2H, H2G, G2G
    flags(int): optional flags
Returns:
    0 if successful)")
        .def("copy_data_2d", &BigMemory::CopyData2D, py::call_guard<py::gil_scoped_release>(), py::arg("params"),
             py::arg("type"), py::arg("flags") = 0, R"(
2D data operation on Big Memory object.

Arguments:
    params(CopyData2DParams): parameters of 2D copy
    type(BmCopyType): copy type, L2G, G2L, G2H, H2G, G2G
    flags(int): optional flags
Returns:
    0 if successful)");
}
}  // namespace

PYBIND11_MODULE(_pymf_smem, m)
{
    DefineSmemFunctions(m);

    auto shm = m.def_submodule("shm", "Share Memory Module.");
    auto bm = m.def_submodule("bm", "Big Memory Module.");

    DefineShmConfig(shm);
    DefineShmClass(shm);

    DefineBmConfig(bm);
    DefineBmClass(bm);
    DefineBmCopyData2DParams(bm);
}