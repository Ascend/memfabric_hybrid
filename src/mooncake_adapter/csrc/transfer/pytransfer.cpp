/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Create Date : 2025
 */
#include "pytransfer.h"

#include <iostream>
#include <pybind11/stl.h>
#include "transfer_util.h"
#include "adapter_logger.h"
#include <thread>
#include <chrono>

using namespace ock::adapter;

namespace py = pybind11;

static const char *PY_TRANSFER_LIB_VERSION = "library version: 1.0.0"
                                 ", build time: " __DATE__ " " __TIME__
                                 ", commit: " STR2(GIT_LAST_COMMIT);

TransferAdapterPy::TransferAdapterPy() {
}

TransferAdapterPy::~TransferAdapterPy() {
}

int TransferAdapterPy::Initialize(const char *storeUrl, const char *uniqueId, const char *role, uint32_t deviceId)
{
    const std::chrono::seconds WAIT_TIME(10);
    if (strcmp(role, "Prefill") != 0 && strcmp(role, "Decode") != 0) {
        ADAPTER_LOG_ERROR("The value of role is invalid. Expected 'Prefill' or 'Decode.");
        return -1;
    }
    smem_trans_config_t config;
    ADAPTER_LOG_INFO("Begin to initialize trans");
    int32_t ret = smem_trans_config_init(&config);
    if (ret != 0) {
        ADAPTER_LOG_ERROR("Failed to init smem_trans_config, ret=" << ret);
        return ret;
    }
    config.role = (strcmp(role, "Prefill") == 0) ? SMEM_TRANS_SENDER : SMEM_TRANS_RECEIVER;
    config.deviceId = deviceId;
    ret = smem_trans_init(&config);
    if (ret != 0) {
        ADAPTER_LOG_ERROR("Failed to init smem_trans, ret=" << ret);
        return ret;
    }
    handle_ = smem_trans_create(storeUrl, uniqueId, &config);
    if (handle_ == nullptr) {
        ADAPTER_LOG_ERROR("smem trans create failed.");
        return -1;
    }
    std::this_thread::sleep_for(WAIT_TIME);  // 等待初始化完成
    return 0;
}

int TransferAdapterPy::GetRpcPort()
{
    int rpcPort = static_cast<int>(findAvailableTcpPort());
    ADAPTER_LOG_INFO("Get rpcPort is " << rpcPort);
    return rpcPort;
}

int TransferAdapterPy::TransferSyncWrite(const char *destSession,
                                         uintptr_t buffer,
                                         uintptr_t peer_buffer_address,
                                         size_t length)
{
    // 将uintptr_t类型的地址转换为指针类型
    const void *srcAddress = reinterpret_cast<const void*>(buffer);
    void *destAddress = reinterpret_cast<void*>(peer_buffer_address);
    
    // 调用底层SMEM API
    int ret = smem_trans_write(handle_, srcAddress, destSession, destAddress, length);
    if (ret != 0) {
        ADAPTER_LOG_ERROR("SMEM API smem_trans_write happen error, ret=" << ret);
    }
    return ret;
}

int TransferAdapterPy::BatchTransferSyncWrite(const char *destSession,
                                              std::vector<uintptr_t> buffers,
                                              std::vector<uintptr_t> peer_buffer_addresses,
                                              std::vector<size_t> lengths)
{
    // 检查向量大小是否一致
    if (buffers.size() != peer_buffer_addresses.size() ||
        buffers.size() != lengths.size() || buffers.size() > UINT32_MAX) {
        ADAPTER_LOG_ERROR("Buffers, peer_buffer_addresses and lengths is not equal or too long.");
        return -1;
    }
    
    // 转换向量数据为C风格数组
    const size_t batchSize = buffers.size();
    std::vector<const void*> srcAddresses(batchSize);
    std::vector<void*> destAddresses(batchSize);
    std::vector<size_t> dataSizes(batchSize);
    
    // 填充转换后的数组
    for (size_t i = 0; i < batchSize; ++i) {
        srcAddresses[i] = reinterpret_cast<const void*>(buffers[i]);
        destAddresses[i] = reinterpret_cast<void*>(peer_buffer_addresses[i]);
        dataSizes[i] = lengths[i];
    }
    
    // 调用底层批量写入API
    int ret = smem_trans_batch_write(
        handle_,
        srcAddresses.data(),
        destSession,
        destAddresses.data(),
        dataSizes.data(),
        static_cast<uint32_t>(batchSize)
    );
    if (ret != 0) {
        ADAPTER_LOG_ERROR("SMEM API smem_trans_batch_write happen error, ret=" << ret);
    }
    return ret;
}

int TransferAdapterPy::RegisterMemory(uintptr_t buffer_addr, size_t capacity)
{
    char *buffer = reinterpret_cast<char *>(buffer_addr);
    return smem_trans_register_mem(handle_, buffer, capacity, 0);
}

int TransferAdapterPy::UnregisterMemory(uintptr_t buffer_addr)
{
    char *buffer = reinterpret_cast<char *>(buffer_addr);
    return smem_trans_deregister_mem(handle_, buffer);
}

int TransferAdapterPy::BatchRegisterMemory(std::vector<uintptr_t> buffer_addrs,
    std::vector<size_t> capacities)
{
    if (buffer_addrs.size() != capacities.size()) {
        ADAPTER_LOG_ERROR("Size of buffer_addrs and capacities is not equal.");
        return -1;
    }

    uint32_t count = static_cast<uint32_t>(buffer_addrs.size());
    std::vector<void*> registerAddrs(count);

    for (size_t i = 0; i < count; ++i) {
        registerAddrs[i] = reinterpret_cast<void*>(buffer_addrs[i]);
    }

    return smem_trans_batch_register_mem(handle_, registerAddrs.data(), capacities.data(), count, 0);
}

void TransferAdapterPy::TransferDestroy()
{
    smem_trans_destroy(handle_, 0);
    handle_ = nullptr;
}

void TransferAdapterPy::UnInitialize()
{
    smem_trans_uninit(0);
}

void DefineAdapterFunctions(py::module_ &m)
{
    m.def("create_config_store", &PyTransferConfigStore, py::call_guard<py::gil_scoped_release>(), py::arg("store_url"));

    m.def("set_log_level", &PyTransferSetLogLevel, py::call_guard<py::gil_scoped_release>(), py::arg("level"), R"(
set log print level.

Arguments:
    level(int): log level, 0:debug 1:info 2:warn 3:error)");

    m.doc() = PY_TRANSFER_LIB_VERSION;
}

PYBIND11_MODULE(_pymf_transfer, m) {
    py::enum_<TransferAdapterPy::TransferOpcode> transfer_opcode(
        m, "TransferOpcode", py::arithmetic());
    transfer_opcode.value("Read", TransferAdapterPy::TransferOpcode::READ)
        .value("Write", TransferAdapterPy::TransferOpcode::WRITE)
        .export_values();

    DefineAdapterFunctions(m);

    auto adaptor_cls =
        py::class_<TransferAdapterPy>(m, "TransferEngine")
            .def(py::init<>())
            .def("initialize", &TransferAdapterPy::Initialize, py::call_guard<py::gil_scoped_release>(),
                 py::arg("store_url"), py::arg("session_id"), py::arg("role"), py::arg("device_id"))
            .def("get_rpc_port", &TransferAdapterPy::GetRpcPort, py::call_guard<py::gil_scoped_release>())
            .def("transfer_sync_write", &TransferAdapterPy::TransferSyncWrite, py::call_guard<py::gil_scoped_release>(),
                 py::arg("dest_session"), py::arg("buffer"), py::arg("peer_buffer"), py::arg("length"))
            .def("batch_transfer_sync_write", &TransferAdapterPy::BatchTransferSyncWrite,
                 py::call_guard<py::gil_scoped_release>(), py::arg("dest_session"), py::arg("buffers"),
                 py::arg("peer_buffers"), py::arg("lengths"))
            .def("register_memory", &TransferAdapterPy::RegisterMemory, py::call_guard<py::gil_scoped_release>(),
                 py::arg("buffer_addr"), py::arg("capacity"))
            .def("unregister_memory", &TransferAdapterPy::UnregisterMemory, py::call_guard<py::gil_scoped_release>(),
                 py::arg("buffer_addr"))
            .def("batch_register_memory", &TransferAdapterPy::BatchRegisterMemory,
                 py::call_guard<py::gil_scoped_release>(), py::arg("buffer_addrs"), py::arg("capacities"))
            .def("destroy", &TransferAdapterPy::TransferDestroy, py::call_guard<py::gil_scoped_release>())
            .def("unInitialize", &TransferAdapterPy::UnInitialize, py::call_guard<py::gil_scoped_release>());

    adaptor_cls.attr("TransferOpcode") = transfer_opcode;
}
