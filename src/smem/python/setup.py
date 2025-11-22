#!/usr/bin/env python
# coding=utf-8
# Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
# This file is a part of the CANN Open Software.
# Licensed under CANN Open Software License Agreement Version 1.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.

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
