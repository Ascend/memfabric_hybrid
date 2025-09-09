#!/usr/bin/env python
# coding=utf-8
# Copyright: (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.

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
    name="mf_adapter",
    version=current_version,
    author="",
    author_email="",
    description="python api for mf_adapter",
    packages=find_namespace_packages(exclude=("tests*",)),
    url="https://gitee.com/ascend/memfabric_hybrid",
    license="Apache License Version 2.0",
    python_requires=">=3.7",
    package_data={"mf_adapter": ["_pymf_transfer.cpython*.so",
                  "lib/libmf_*.so", "VERSION"]},
    distclass=BinaryDistribution,
)
