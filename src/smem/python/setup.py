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

"""python api for mf_smem."""

import os

import setuptools
from setuptools import find_namespace_packages
from setuptools.dist import Distribution

# 消除whl压缩包的时间戳差异
os.environ['SOURCE_DATE_EPOCH'] = '0'

current_version = os.getenv('VERSION', '1.0.0')


class BinaryDistribution(Distribution):
    """Distribution which always forces a binary package with platform name"""

    def has_ext_modules(self):
        return True


pkgs = find_namespace_packages()
print(pkgs)


setuptools.setup(
    name="mf_smem",
    version=current_version,
    author="",
    author_email="",
    description="python api for smem",
    packages=find_namespace_packages(exclude=("tests*",)),
    url="https://gitee.com/ascend/memfabric_hybrid",
    license="Apache License Version 2.0",
    python_requires=">=3.7",
    package_data={"mf_smem": ["_pymf_smem.cpython*.so", "lib/**", "VERSION"]},
    distclass=BinaryDistribution
)
