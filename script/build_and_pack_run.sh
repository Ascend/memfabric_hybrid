#!/bin/bash
# Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
# MemFabric_Hybrid is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#          http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.

set -e
readonly ROOT_PATH=$(dirname $(readlink -f "$0"))
CURRENT_DIR=$(pwd)

BUILD_MODE="RELEASE"
BUILD_PYTHON="ON"
XPU_TYPE="NPU"
BUILD_TEST="OFF"
BUILD_HCOM="OFF"
BUILD_HCOM_WITH_RDMA="ON"
BUILD_HCOM_WITH_UB="OFF"
BUILD_ETCD_BACKEND="OFF"
BUILD_TOOL="cmake"
BUILD_AICPU_KERNEL_RUN="OFF"

show_help() {
    echo "Usage: $0 [options]"
    echo "Options:"
    echo "  --build_mode <mode>         Set build mode (RELEASE/DEBUG/ASAN), default: RELEASE"
    echo "  --build_python <ON/OFF>     Enable/disable Python build, default: ON"
    echo "  --xpu_type <GPU/NPU/NONE>   Set xpu dependency(GPU:CUDA, NPU:CANN), set none without xpu, default: NPU"
    echo "  --build_test <ON/OFF>       Enable/disable build and package test utilities and examples, default: OFF"
    echo "  --build_hcom <ON/OFF>       Enable/disable build and package hcom, default: OFF"
    echo "  --build_hcom_rdma <ON/OFF>  Enable/disable build and package hcom with rdma, default: ON"
    echo "  --build_hcom_ub <ON/OFF>    Enable/disable build and package hcom with ub, default: OFF"
    echo "  --build_etcd_backend <ON/OFF> Enable/disable build and package etcd backend so, default: OFF"
    echo "  --build_tool <cmake/bazel>  Set build tool (cmake/bazel), default: cmake"
    echo "  --build_aicpu_kernel_run <ON/OFF>  Enable/disable build AICPU kernel run package, default: OFF"
    echo "  --help                      Show this help message"
    echo ""
    echo "Example:"
    echo "  $0 --build_mode DEBUG --build_python ON"
    echo ""
}

while [[ "$#" -gt 0 ]]; do
    case "$1" in
        --build_mode)
            BUILD_MODE="$2"
            shift 2
            ;;
        --build_python)
            BUILD_PYTHON="$2"
            shift 2
            ;;
        --xpu_type)
            XPU_TYPE="$2"
            shift 2
            ;;
        --build_test)
            BUILD_TEST="$2"
            shift 2
            ;;
        --build_hcom)
            BUILD_HCOM="$2"
            shift 2
            ;;
        --build_hcom_rdma)
            BUILD_HCOM_WITH_RDMA="$2"
            shift 2
            ;;
        --build_hcom_ub)
            BUILD_HCOM_WITH_UB="$2"
            shift 2
            ;;
        --build_etcd_backend)
            BUILD_ETCD_BACKEND="$2"
            shift 2
            ;;
        --build_tool)
            BUILD_TOOL="$2"
            shift 2
            ;;
        --build_aicpu_kernel_run|--build-aicpu-kernel-run)
            BUILD_AICPU_KERNEL_RUN="$2"
            shift 2
            ;;
        --help)
            show_help
            exit 0
            ;;
        *)
            echo "Unknown argument: $1"
            echo ""
            show_help
            exit 1
            ;;
    esac
done

echo "BUILD_MODE: $BUILD_MODE"
echo "BUILD_PYTHON: $BUILD_PYTHON"
echo "XPU_TYPE: $XPU_TYPE"
echo "BUILD_TEST: $BUILD_TEST"
echo "BUILD_HCOM: $BUILD_HCOM"
echo "BUILD_HCOM_RDMA: $BUILD_HCOM_WITH_RDMA"
echo "BUILD_HCOM_WITH_UB: $BUILD_HCOM_WITH_UB"
echo "BUILD_ETCD_BACKEND: $BUILD_ETCD_BACKEND"
echo "BUILD_TOOL: $BUILD_TOOL"
echo "BUILD_AICPU_KERNEL_RUN: $BUILD_AICPU_KERNEL_RUN"

cd ${ROOT_PATH}

bash build.sh "${BUILD_MODE}" OFF OFF "${BUILD_PYTHON}" ON "${XPU_TYPE}" "${BUILD_TEST}" "${BUILD_HCOM}" "${BUILD_HCOM_WITH_RDMA}" "${BUILD_HCOM_WITH_UB}" "${BUILD_ETCD_BACKEND}" "${BUILD_TOOL}"

bash run_pkg_maker/make_run.sh "${BUILD_TEST}" "${XPU_TYPE}" "${BUILD_PYTHON}" "${BUILD_HCOM}" "${BUILD_ETCD_BACKEND}"

# ---------------------------------------------------------------
# Generate cann_hybm_kernel_version metadata for AICPU kernel run package
# ---------------------------------------------------------------
generate_aicpu_kernel_version() {
    local proj_root="$1"
    local ver_file="${proj_root}/output/hybm/aicpu_kernel/cann_hybm_kernel_version"
    local version_src="${proj_root}/output/VERSION"
    mkdir -p "$(dirname "${ver_file}")"

    if [[ ! -f "${version_src}" ]]; then
        echo "Error: ${version_src} not found. build.sh must generate it first." >&2
        exit 1
    fi
    cp "${version_src}" "${ver_file}"
}

if [ "${BUILD_AICPU_KERNEL_RUN}" = "ON" ]; then
    echo ""
    echo "=== Building AICPU kernel run package ==="
    generate_aicpu_kernel_version "$(cd "${ROOT_PATH}/.." && pwd)"
    bash kernel/make_kernel_run.sh
    echo "=== AICPU kernel run package complete ==="
fi

cd ${CURRENT_DIR}
