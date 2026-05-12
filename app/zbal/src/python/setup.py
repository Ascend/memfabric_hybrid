# Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
# ZBAL is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#          http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.
import logging
import os
import glob
from pathlib import Path
import sysconfig
import subprocess
import platform
import shutil

import setuptools
from setuptools import setup
from torch.utils.cpp_extension import CppExtension, BuildExtension
from wheel.bdist_wheel import bdist_wheel

import torch
import torch_npu

logger = logging.getLogger(__name__)
logging.basicConfig(level=logging.INFO)


def _find_ascend_home_dir():
    """
    Get the home dir of ascend for kernel compiling.
    The default path is:
    /usr/local/Ascend/ascend-toolkit/latest
    """
    env = os.environ.get("ASCEND_TOOLKIT_HOME")
    if env:
        return env
    default_dir = "/usr/local/Ascend/ascend-toolkit/latest"
    if os.path.isdir(default_dir):
        return default_dir
    alter = "/usr/local/Ascend/ascend-toolkit"
    latest = os.path.join(alter, "latest")
    return latest if os.path.isdir(latest) else default_dir


def _find_python_include():
    return sysconfig.get_path('include')


def _get_version(version_dir):
    with open(f"{version_dir}/VERSION", "r", encoding="utf-8") as f:
        version = f.read().strip()
        return version


def _check_env_flag(name: str, default: str = "") -> bool:
    return os.getenv(name, default).upper() in ["ON", "1", "YES", "TRUE", "Y"]


is_manylinux = _check_env_flag("IS_MANYLINUX", "FALSE")
ascend_home = Path(_find_ascend_home_dir()).resolve()
python_include_dir = Path(_find_python_include()).resolve()
torch_dir = Path(os.path.dirname(torch.__file__)).resolve()
torch_npu_dir = Path(os.path.dirname(torch_npu.__file__)).resolve()
repo_root = Path(__file__).parent.parent.parent.parent.parent  # sgl-kernel-npu/
zbal_root = repo_root / "app/zbal/"
versoin = _get_version(version_dir=zbal_root)

# allocator compile inputs
include_dirs = [

    f"{zbal_root}/",
    f"{zbal_root}/third_party/ska",
    f"{zbal_root}/third_party/mstx",
    f"{zbal_root}/src/include",
    f"{zbal_root}/src/csrc/",
    f"{zbal_root}/src/csrc/operators",
    f"{zbal_root}/src/csrc/operators/npu",
    f"{zbal_root}/src/csrc/common",
    f"{zbal_root}/src/csrc/dma",
    f"{zbal_root}/src/csrc/sma",
    f"{zbal_root}/src/csrc/under_api/cann",
    f"{zbal_root}/src/csrc/under_api/memfabric",
    f"{zbal_root}/src/csrc/bootstrap",
    f"{zbal_root}/src/csrc/bootstrap/memory",
    f"{zbal_root}/src/csrc/bootstrap/memory/memfabric",
    f"{zbal_root}/src/csrc/adaptor/pytorch_npu/",
    f"{zbal_root}/src/csrc/adaptor/deepep/",
]

library_dirs = [
    f"{torch_dir}/lib",
    f"{torch_npu_dir}/lib",
    f"{ascend_home}/lib64",
    sysconfig.get_config_var("LIBDIR"),
    f"{zbal_root}/output/",
]

csrc_dir = repo_root / "app" / "zbal" / "src" / "csrc"
sources = ([f"{csrc_dir}/zbal_pybind.cpp"] + \
           glob.glob(str(csrc_dir / "sma" / "*.cpp")) + \
           glob.glob(str(csrc_dir / "adaptor" / "pytorch_npu" / "*.cpp")) + \
           glob.glob(str(csrc_dir / "adaptor" / "deepep" / "*.cpp")))

libraries = ["torch", "torch_npu", "c10", "torch_python", "opapi", "zbal_core", "zbal_kernel"]

logger.warning(f"Using ASCEND_TOOLKIT_HOME at: {ascend_home}")
logger.warning(f"{include_dirs=}")
logger.warning(f"{sources=}")
logger.warning(f"{library_dirs=}")
logger.warning(f"{libraries=}")

extra_compile_args = ["-std=c++17", "-hno-unused-parameter", "-lno-unused-function", "-Wno-unused-function",
                      "-Wunused-value", "-Wcast-align",
                      "-Wcast-qual", "-Winvalid-pch", "-Wwrite-strings", "-Wsign-compare", "-Wextra",
                      "-O3", "-fvisibility-inlines-hidden", "-fstack-protector-strong",
                      "-Wl,-z,noexecstack", "-Wl,-z,relro", "-Wl,-z,now", "-fPIE", "-fPIC",
                      "-ftrapv",
                      "-isystem", f"{python_include_dir}",
                      "-isystem", f"{ascend_home}/include",
                      "-isystem", f"{ascend_home}/include/experiment/runtime/runtime/",
                      "-isystem", f"{torch_dir}/",
                      "-isystem", f"{torch_dir}/include/torch/csrc/api/include/",
                      "-isystem", f"{torch_dir}/include/torch/csrc/utils/",
                      "-isystem", f"{torch_dir}/include/c10/util/",
                      "-isystem", f"{torch_dir}/include/c10/core/",
                      "-isystem", f"{torch_dir}/include/",
                      "-isystem", f"{torch_dir}/include/ATen/",
                      "-isystem", f"{torch_dir}/include/ATen/detail/",
                      "-isystem", f"{torch_dir}/include/torch/csrc/distributed/",
                      "-isystem", f"{torch_dir}/include/torch/csrc/distributed/c10d/",
                      "-isystem", f"{torch_npu_dir}/include/",
                      "-isystem", f"{torch_npu_dir}/include/torch_npu/csrc/aten/",
                      "-isystem", f"{torch_npu_dir}/include/torch_npu/csrc/core/npu/",
                      ]  # "-fvisibility=hidden"
common_macros = []


class CustomBuildExtension(BuildExtension):
    def build_base_zbal(self):
        # make dir
        cur_dir = os.path.dirname(os.path.abspath(__file__))
        root_dir = Path(cur_dir).parent.parent
        build_dir = os.path.join(f"{root_dir}", "build")
        output_dir = os.path.join(f"{root_dir}", "output")
        shutil.rmtree(build_dir, ignore_errors=True)
        shutil.rmtree(output_dir, ignore_errors=True)
        os.makedirs(build_dir, exist_ok=True)
        os.makedirs(output_dir, exist_ok=True)
        logger.info(f"make build dir:{build_dir}, output dir:{output_dir}")

        # cmake
        build_type = "Release" if is_manylinux else "Debug"
        cmake_cmd = [
            "cmake",
            "..",
            "-DSOC_VERSION=Ascend910_9382",
            "-DBUILD_ZBAL_MODULE_UT=OFF",
            f"-DCMAKE_BUILD_TYPE={build_type}",
            "-DDISABLE_ADAPTOR_COMPILE=ON",
            "-DDISABLE_ALLOCATOR_COMPILE=ON",
            "-DDISABLE_SHARE_COMPILE=ON"
        ]
        result = subprocess.run(cmake_cmd, cwd=build_dir)
        if result.returncode != 0:
            logger.error(f"python cmake exec failed ret code {result.returncode}, msg {result.stderr}")
            raise RuntimeError("cmake exec failed")
        else:
            logger.info("python cmake exec success")

        # make
        make_cmd = [
            "make",
            "-j16"
        ]
        result = subprocess.run(make_cmd, cwd=build_dir)
        if result.returncode != 0:
            logger.error(f"python make exec failed ret code {result.returncode}, msg {result.stderr}")
            raise RuntimeError("make exec failed")
        else:
            logger.info("python make exec success")

        # copy
        static_output = glob.glob(f"{build_dir}/**/*.a", recursive=True)
        for x in static_output:
            static_name = os.path.basename(x)
            dst = f"{output_dir}/{static_name}"
            shutil.copy2(x, dst)
            logger.info(f"copy {x} to {dst}")

    def run(self):
        self.build_base_zbal()
        super().run()


class BuildWheel(bdist_wheel):
    def run(self):
        bdist_wheel.run(self)

        exclude_libraries = [
            # torch
            "libtorch.so", "libtorch_npu.so", "libc10.so",
            "libtorch_python.so", "libtorch_cpu.so",

            "libopapi.so"

        ]

        if is_manylinux:
            file = glob.glob(os.path.join(self.dist_dir, "*-linux_*.whl"))[0]

            auditwheel_cmd = [
                "auditwheel",
                "-v",
                "repair",
                "--plat",
                f"manylinux_2_34_{platform.machine()}",
                "-w",
                self.dist_dir,
                file,
            ]
            for lib in exclude_libraries:
                auditwheel_cmd.extend(["--exclude", lib])

            try:
                subprocess.run(auditwheel_cmd, check=True, stdout=subprocess.PIPE)
            finally:
                os.remove(file)


setup(
    name="zbal-ascend",
    version=versoin,
    ext_modules=[
        CppExtension(
            name="zbal.zbal",  # TORCH_EXTENSION_NAME
            sources=sources,
            include_dirs=include_dirs,
            library_dirs=library_dirs,
            libraries=libraries,
            define_macros=[
                *common_macros,
            ],
            extra_compile_args=extra_compile_args,
            cxx_std=17
        )
    ],
    python_requires=">=3.10",
    packages=setuptools.find_packages(
        include=["zbal", "zbal.*"]
    ),
    cmdclass={'build_ext': CustomBuildExtension,
              'bdist_wheel': BuildWheel},
)
