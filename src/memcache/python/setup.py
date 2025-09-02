#!/usr/bin/env python
# coding=utf-8
# Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.

"""python api for memcache."""

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


setuptools.setup(
    name="memcache",
    version=current_version,
    author="",
    author_email="",
    description="python api for memcache",
    packages=find_namespace_packages(exclude=("tests*",)),
    url="https://gitee.com/ascend/memfabric_hybrid",
    license="Apache License Version 2.0",
    python_requires=">=3.7",
    package_data={"memcache": ["_pymmc.cpython*.so", "lib/**", "VERSION"]},
    distclass=BinaryDistribution
)
