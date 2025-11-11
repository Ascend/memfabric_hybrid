#!/usr/bin/env python
# coding=utf-8
# Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
# This file is a part of the CANN Open Software.
# Licensed under CANN Open Software License Agreement Version 1.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.

"""python api for smem."""

import os
import platform
import glob

import shutil
import setuptools
import subprocess
from setuptools import Extension, setup
from setuptools import find_namespace_packages
from setuptools.dist import Distribution
from setuptools.command.build_ext import build_ext
from wheel.bdist_wheel import bdist_wheel


def check_env_flag(name: str, default: str = "") -> bool:
    return os.getenv(name, default).upper() in ["ON", "1", "YES", "TRUE", "Y"]


def remove_directory(dir_path):
    if os.path.exists(dir_path) and os.path.isdir(dir_path):
        shutil.rmtree(dir_path)


# 消除whl压缩包的时间戳差异
os.environ['SOURCE_DATE_EPOCH'] = '0'

current_version = os.getenv('VERSION', '1.0.0')
is_manylinux = check_env_flag("IS_MANYLINUX", "FALSE")
root_dir = os.path.dirname(os.path.abspath(__file__))

build_open_abi = os.getenv('BUILD_OPEN_ABI', '')
build_mode = os.getenv('BUILD_MODE', '')
build_compiler = os.getenv('BUILD_COMPILER', '')

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

            try:
                subprocess.run(auditwheel_cmd, check=True, stdout=subprocess.PIPE)
            finally:
                os.remove(file)


class CMakeBuildExt(build_ext):
    def run(self):
        root_dir = os.path.join(os.path.dirname(__file__), "../../../")
        build_dir = os.path.join(root_dir, "build")
        install_dir = os.path.join(build_dir, "install")
        os.makedirs(build_dir, exist_ok=True)
        config_mode = "Release"
        if build_mode == "DEBUG":
            config_mode = "Debug"
        subprocess.check_call([
            "cmake",
            f"-S{root_dir}",
            f"-B{build_dir}",
            f"-DCMAKE_INSTALL_PREFIX={install_dir}",
            f"-DCMAKE_BUILD_TYPE={build_mode}",
            f"-DBUILD_OPEN_ABI={build_open_abi}",
            f"-DBUILD_COMPILER=${build_compiler}",
            "-DBUILD_PYTHON=ON",
            "-DBUILD_TESTS=OFF"
        ])
        subprocess.check_call([
            "cmake", "--build", build_dir, 
            "--config", config_mode, "--target", "install",
            "-j8"
        ])
        super().run()

    def build_extension(self, ext):
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
    name="mf_smem",
    version=current_version,
    author="",
    author_email="",
    description="python api for smem",
    packages=find_namespace_packages(exclude=("tests*",)),
    url="https://gitee.com/ascend/memfabric_hybrid",
    license="Apache License Version 2.0",
    python_requires=">=3.7",
    zip_safe=False,
    package_data={"mf_smem": ["_pymf_smem.cpython*.so", "VERSION"]},
    cmdclass={
        "build_ext": CMakeBuildExt,
        "bdist_wheel": BuildWheel,
    },
    distclass=BinaryDistribution,
)
