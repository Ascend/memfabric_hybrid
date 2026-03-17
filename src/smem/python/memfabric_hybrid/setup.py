#!/usr/bin/env python
# coding=utf-8
# Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
# MemFabric_Hybrid is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#          http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.

"""python api for memfabric_hybrid."""

import os
import sys
import platform
import glob
import shutil
import subprocess
from setuptools import Extension, setup
from setuptools import find_namespace_packages
from setuptools.dist import Distribution
from setuptools.command.build_ext import build_ext
from wheel.bdist_wheel import bdist_wheel


def check_env_flag(name: str, default: str = "") -> bool:
    return os.getenv(name, default).upper() in ["ON", "1", "YES", "TRUE", "Y"]


# 消除whl压缩包的时间戳差异
os.environ["SOURCE_DATE_EPOCH"] = "0"

current_version = os.getenv("MEMFABRIC_VERSION")
if not current_version:
    print("Error: MEMFABRIC_VERSION environment variable must be set.", file=sys.stderr)
is_manylinux = check_env_flag("IS_MANYLINUX", "FALSE")
build_open_abi = os.getenv("BUILD_OPEN_ABI", "OFF")
build_mode = os.getenv("BUILD_MODE", "RELEASE")
enable_ptracer = os.getenv("ENABLE_PTRACER", "ON")
xpu_type = os.getenv("XPU_TYPE", "NPU")
build_hcom = os.getenv("MF_BUILD_HCOM", "OFF")
build_hcom_with_rdma = os.getenv("MF_BUILD_HCOM_WITH_RDMA", "ON")
build_hcom_with_ub = os.getenv("MF_BUILD_HCOM_WITH_UB", "OFF")
build_etcd_backend = os.getenv("BUILD_ETCD_BACKEND", "OFF")
build_tool = os.getenv("BUILD_TOOL", "cmake")

if xpu_type not in ("NPU", "NONE", "GPU"):
    raise ValueError("XPU_TYPE must be exactly NPU, NONE, or GPU")
if xpu_type == "NONE":
    current_version += "+cpu"
elif xpu_type == "GPU":
    current_version += "+gpu"

if build_tool not in ("cmake", "bazel"):
    raise ValueError("BUILD_TOOL must be exactly cmake or bazel")

class BinaryDistribution(Distribution):
    """Distribution which always forces a binary package with platform name"""

    def has_ext_modules(self):
        return True


class BuildWheel(bdist_wheel):
    def run(self):
        bdist_wheel.run(self)

        if is_manylinux:
            file = glob.glob(os.path.join(self.dist_dir, "*-linux_*.whl"))[0]

            auditwheel_cmd = [
                "auditwheel",
                "-v",
                "repair",
                "--plat",
                f"manylinux_2_27_{platform.machine()}",
                "--plat",
                f"manylinux_2_28_{platform.machine()}",
                "-w",
                self.dist_dir,
                file,
            ]
            subprocess.check_call(auditwheel_cmd)
            os.remove(file)


class CMakeBuildExt(build_ext):
    def run(self):
        root_dir = os.path.abspath(
            os.path.join(os.path.dirname(__file__), "../../../../")
        )
        build_dir = os.path.abspath(os.path.join(root_dir, "build"))
        install_dir = os.path.abspath(os.path.join(build_dir, "install"))
        os.makedirs(build_dir, exist_ok=True)
        config_mode = "Release"
        if build_mode == "DEBUG":
            config_mode = "Debug"
        if build_tool == "cmake":
            subprocess.check_call(
                [
                    "cmake",
                    f"-S{root_dir}",
                    f"-B{build_dir}",
                    f"-DCMAKE_INSTALL_PREFIX={install_dir}",
                    f"-DCMAKE_BUILD_TYPE={build_mode}",
                    f"-DBUILD_OPEN_ABI={build_open_abi}",
                    f"-DENABLE_PTRACER={enable_ptracer}",
                    f"-DXPU_TYPE={xpu_type}",
                    "-DBUILD_PYTHON=ON",
                    "-DBUILD_UT=OFF",
                ]
            )
            subprocess.check_call(
                [
                    "cmake",
                    "--build",
                    build_dir,
                    "--config",
                    config_mode,
                    "--target",
                    "install",
                    "-j8",
                ]
            )
        else:
            bazel_cmd = ["bazel", "build", "//..."]
            if build_mode == "DEBUG":
                bazel_cmd.append("--compilation_mode=dbg")
            else:
                bazel_cmd.append("--compilation_mode=opt")

            if (build_open_abi == "ON"):
                bazel_cmd.append("--copt=-D_GLIBCXX_USE_CXX11_ABI=1")
            else:
                bazel_cmd.append("--copt=-D_GLIBCXX_USE_CXX11_ABI=0")

            if (enable_ptracer == "ON"):
                bazel_cmd.append("--copt=-DENABLE_PTRACER")

            if xpu_type == "NPU":
                bazel_cmd.append("--copt=-DASCEND_NPU")
            elif xpu_type == "GPU":
                bazel_cmd.append("--copt=-DNVIDIA_GPU")
            else:
                bazel_cmd.append("--copt=-DNO_XPU")

            if build_hcom == "ON":
                bazel_cmd.append("--define=build_with_hcom=1")
    
                if build_hcom_with_rdma == "OFF":
                    bazel_cmd.append("--define=hcom_enable_rdma=0")
                if build_hcom_with_ub == "ON":
                    bazel_cmd.append("--define=hcom_enable_ub=1")

            if build_etcd_backend == "OFF":
                bazel_cmd.append("--build_tag_filters=-enable_etcd_client")
            
            bazel_cmd.append("--explain=explain.log")
            bazel_cmd.append("--verbose_explanations")

            print(bazel_cmd)
            subprocess.check_call(
                bazel_cmd
            )
        super().run()

    def build_extension(self, ext):
        print("skip the step cause _pymf_hybrid is already builded")
        root_dir = os.path.join(os.path.dirname(__file__), "..")
        install_dir = os.path.join(root_dir, "build", "install")
        lib_dir = os.path.join(install_dir, "lib")
        include_dir = os.path.join(install_dir, "include")

        ext.extra_compile_args = ["-std=c++11"]
        ext.include_dirs.append(include_dir)
        ext.library_dirs.append(lib_dir)
        ext.libraries.append("mf_smem")
        super().build_extension(ext)
        ext_dir = os.path.dirname(self.get_ext_fullpath(ext.name))
        shutil.copy(os.path.join(lib_dir, "libmf_smem.so"), ext_dir)
        shutil.copy(os.path.join(lib_dir, "libmf_hybm_core.so"), ext_dir)


pkgs = find_namespace_packages()
print(pkgs)

setup(
    name="memfabric_hybrid",
    version=current_version,
    author="",
    author_email="",
    description="python api for memfabric hybrid",
    packages=find_namespace_packages(exclude=("tests*",)),
    url="https://gitcode.com/Ascend/memfabric_hybrid",
    license="Mulan PSL v2",
    python_requires=">=3.7",
    zip_safe=False,
    package_data={
        "memfabric_hybrid": [
            "_pymf_hybrid*.so",
            "_pymf_transfer*.so",
            "lib/lib*.so",
            "include/smem/host/*.h",
            "include/smem/device/*.h",
            "include/hybm/*.h",
            "VERSION",
        ]
    },
    cmdclass={
        "build_ext": CMakeBuildExt,
        "bdist_wheel": BuildWheel,
    },
    distclass=BinaryDistribution,
)
