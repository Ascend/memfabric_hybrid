/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ZBAL is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/chrono.h>

#include "zbal_pytorch_process_group_impl.h"
#include "zbal_deepep.h"
#include "zbal_def.h"
#include "zbal_bootstrap.h"
#include "zbal.h"

// for mem_allocator
#include "zbal_mem_allocator.h"
#include "zbal_pytorch_c10_dma.h"
#include "zbal_sma.h"
#include "zbal_sma_config.h"

namespace py = pybind11;
using namespace zbal::adaptor::pytorch_npu;
using ZOptions = ProcessGroupZBAL::Options;
using CBackend = c10d::Backend;

static zbal_bootstrap_output_t output;

int32_t zbal_bootstrap_wrapper(zbal_bootstrap_options_t &opt)
{
    return zbal_bootstrap(&opt, &output);
}

void pybind11_enums(py::module_ &m)
{
    py::enum_<zbal_bootstrap_type_t>(m, "ZBALBootstrapType")
        .value("BOOT_BY_MEMFABRIC", zbal_bootstrap_type_t::BOOT_BY_MEMFABRIC);

    py::enum_<zbal_backend_t>(m, "ZBALBackendType").value("ZBAL_ASCEND_NPU", zbal_backend_t::ZBAL_ASCEND_NPU);
}

void pybind11_bootstrap_options(py::module_ &m)
{
    py::class_<zbal_bootstrap_options_t>(m, "ZBALBootstrapOption")
        .def(py::init<>())
        .def_readwrite("flags", &zbal_bootstrap_options_t::flags)
        .def_readwrite("btType", &zbal_bootstrap_options_t::btType)
        .def_readwrite("worldSize", &zbal_bootstrap_options_t::worldSize)
        .def_readwrite("rankId", &zbal_bootstrap_options_t::rankId)
        .def_readwrite("deviceId", &zbal_bootstrap_options_t::deviceId)
        .def_readwrite("startConfigServer", &zbal_bootstrap_options_t::startConfigServer)
        .def_readwrite("deviceMemorySize", &zbal_bootstrap_options_t::deviceMemorySize)
        .def_readwrite("dataOperationType", &zbal_bootstrap_options_t::dataOperationType)
        .def_readwrite("commMetaSpaceSize", &zbal_bootstrap_options_t::commMetaSpaceSize)
        .def_readwrite("commGroupCap", &zbal_bootstrap_options_t::commGroupCap)
        .def_property(
            "ipPort", [](const zbal_bootstrap_options_t &opt) { return std::string(opt.ipPort, strlen(opt.ipPort)); },
            [](zbal_bootstrap_options_t &opt, const std::string &ipPort) {
                if (ipPort.size() >= ZBAL_MAX_IPPORT_LEN) {
                    throw std::runtime_error("ipPort is too long");
                }
                std::copy(ipPort.begin(), ipPort.end(), opt.ipPort);
                opt.ipPort[ipPort.size()] = '\0';
            });
}

void pybind11_comm_property(py::module_ &m)
{
    py::class_<zbal_comm_property_t>(m, "ZBALCommProperty")
        .def(py::init<>())
        .def_readwrite("backendType", &zbal_comm_property_t::backendType)
        .def_readwrite("isWorldGroup", &zbal_comm_property_t::isWorldGroup)
        .def_readwrite("groupSize", &zbal_comm_property_t::groupSize)
        .def_readwrite("groupRankId", &zbal_comm_property_t::groupRankId)
        .def_readwrite("symmetricMetaGva", &zbal_comm_property_t::symmetricMetaGva)
        .def_property(
            "myGVA", [](const zbal_comm_property_t &prop) { return reinterpret_cast<uintptr_t>(prop.myGVA); },
            [](zbal_comm_property_t &prop, uintptr_t myGVA) { prop.myGVA = reinterpret_cast<void *>(myGVA); })
        .def_property(
            "myMetaGVA", [](const zbal_comm_property_t &prop) { return reinterpret_cast<uintptr_t>(prop.myMetaGVA); },
            [](zbal_comm_property_t &prop, uintptr_t myMetaGVA) {
                prop.myMetaGVA = reinterpret_cast<void *>(myMetaGVA);
            })
        .def_property(
            "myMetaGVAForOpParam",
            [](const zbal_comm_property_t &prop) { return reinterpret_cast<uintptr_t>(prop.myMetaGVAForOpParam); },
            [](zbal_comm_property_t &prop, uintptr_t myMetaGVAForOpParam) {
                prop.myMetaGVAForOpParam = reinterpret_cast<void *>(myMetaGVAForOpParam);
            })
        .def_property(
            "myMetaGVAForOpExchange",
            [](const zbal_comm_property_t &prop) { return reinterpret_cast<uintptr_t>(prop.myMetaGVAForOpExchange); },
            [](zbal_comm_property_t &prop, uintptr_t myMetaGVAForOpExchange) {
                prop.myMetaGVAForOpExchange = reinterpret_cast<void *>(myMetaGVAForOpExchange);
            })
        .def_readwrite("sizeOfMetaArea", &zbal_comm_property_t::sizeOfMetaArea)
        .def_readwrite("sizeOfMetaForOpParam", &zbal_comm_property_t::sizeOfMetaForOpParam)
        .def_readwrite("sizeOfMetaForAddressExchange", &zbal_comm_property_t::sizeOfMetaForAddressExchange)
        .def_readwrite("localDeviceMemSize", &zbal_comm_property_t::localDeviceMemSize)
        .def_property(
            "name", [](const zbal_comm_property_t &prop) { return std::string(prop.name); },
            [](zbal_comm_property_t &prop, const std::string &name) {
                if (name.size() >= ZBAL_COMM_NAME_MAX) {
                    throw std::runtime_error("name is too long");
                }
                std::copy(name.begin(), name.end(), prop.name);
                prop.name[name.size()] = '\0';
            });
}

void pybind11_process_group(py::module_ &m)
{
    auto group =
        py::class_<ProcessGroupZBALImpl, CBackend, c10::intrusive_ptr<ProcessGroupZBALImpl>>(m, "ProcessGroupZBAL")
            .def(py::init<const c10::intrusive_ptr<::c10d::Store> &, int, int, c10::intrusive_ptr<ZOptions>>(),
                 py::call_guard<py::gil_scoped_release>())
            .def("get_zbal_comm_name", &ProcessGroupZBALImpl::getZBALCommName)
            .def("get_hccl_comm_name",
                 [](ProcessGroupZBALImpl &pg, int rankId, py::args args, py::kwargs kwargs) -> std::string {
                     (void)rankId;
                     (void)args;
                     (void)kwargs;
                     return pg.getZBALCommName(); // for compatibility profiler
                 })
            .def("init_zbal_comm_meta", &ProcessGroupZBALImpl::initCommunicator);

    py::class_<ZOptions, CBackend::Options, c10::intrusive_ptr<ZOptions>>(group, "Options")
        .def(py::init<>())
        .def_readwrite("op_timeout", &ZOptions::opTimeout)
        .def_readwrite("is_high_priority_stream", &ZOptions::isHighPriorityStream)
        .def_readwrite("global_ranks_in_group", &ZOptions::globalRanksInGroup)
        .def_readwrite("group_id", &ZOptions::groupId);
}

void pybind11_definitions(py::module_ &m)
{
    pybind11_enums(m);
    pybind11_bootstrap_options(m);
    pybind11_comm_property(m);
    pybind11_process_group(m);
}

void pybind11_functions(py::module_ &m)
{
    m.def("zbal_bootstrap", &zbal_bootstrap_wrapper);
    m.def("zbal_unbootstrap", &zbal_unbootstrap);
    m.def("zbal_set_logger_level", &zbal_set_logger_level);
    m.def("zbal_version", &zbal_version);

    // communicator
    m.def("zbal_comm_get_global", []() -> uintptr_t { return reinterpret_cast<uintptr_t>(zbal_comm_get_global()); });
    m.def("zbal_comm_get_by_name",
          [](const char *name) -> uintptr_t { return reinterpret_cast<uintptr_t>(zbal_comm_get_by_name(name)); });
    m.def("zbal_comm_get_property", [](uintptr_t comm) {
        zbal_comm_property_t prop;
        zbal_comm_get_property(reinterpret_cast<zbal_comm_t>(comm), &prop);
        return prop;
    });
    m.def("zbal_comm_destroy_all", &zbal_comm_destroy_all);
    m.def("zbal_comm_destroy",
          [](uintptr_t comm, uint32_t flags) { return zbal_comm_destroy(reinterpret_cast<zbal_comm_t>(comm), flags); });
}

void pybind11_bootstrap(py::module_ &m)
{
    pybind11_definitions(m);
    pybind11_functions(m);
}

void pybind11_allocator(pybind11::module_ &m)
{
    m.doc() = "ZBAL Allocator Stats API";

    m.def(
        "record_memory_history",
        [](std::optional<std::string> enabled, int64_t max_entries) {
            if (!zbal::sma::SMAConfig::use_sma_allocator()) {
                dma_record_memory_history(enabled, max_entries);
            } else {
                sma_record_memory_history(enabled, max_entries);
            }
            return;
        },
        "begin record memory with history");

    m.def(
        "get_heap_stats",
        [](int device) {
            size_t in_used_size = 0;
            size_t total_size = 0;

            if (!zbal::sma::SMAConfig::use_sma_allocator()) {
                dma_get_heap_stats(in_used_size, total_size, device);
            } else {
                sma_get_heap_stats(in_used_size, total_size, device);
            }

            return std::make_tuple(in_used_size, total_size);
        },
        pybind11::arg("device") = -1,
        "get heap stats, return (used_size, total_size), return zero if heap is not inited");

    m.def(
        "dump_snapshot",
        []() {
            if (!zbal::sma::SMAConfig::use_sma_allocator()) {
                return dma_dump_snapshot();
            } else {
                return sma_dump_snapshot();
            }
        },
        "dump snapshot, return pkl dict");

    m.def(
        "simulate_init", [](int64_t addr, int64_t size) { zbal_simulate_init(addr, size); },
        "simulate_init on sma/dma heap, no actual memory allocate");

    m.def(
        "is_mix_alloc", []() { return zbal::sma::SMAConfig::use_vmm_for_static_memory(); },
        "check whether allocator is using vmm mix mode");
}

PYBIND11_MODULE(zbal, m)
{
    m.doc() = "zbal package";

    auto allocator = m.def_submodule("allocator", "zbal allocator");
    auto deepep_adaptor = m.def_submodule("deepep_adaptor", "zbal deepep adaptor");

    pybind11_allocator(allocator);
    pybind11_deepep_adaptor(deepep_adaptor);
    pybind11_bootstrap(m);
}