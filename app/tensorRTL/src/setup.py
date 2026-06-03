#!/usr/bin/env python
from setuptools import setup, find_packages

setup(
    name="tensor_rtl",
    version="0.1.0",
    description="TensorRTL: library for parallel tensor redistribution",
    author="Huawei - Ascend",
    author_email="",
    packages=find_packages(include=['tensor_rtl', 'tensor_rtl.*']),
    install_requires=[
        "numpy>=1.20",
        "torch>=2.6.0",
    ],
    python_requires=">=3.8",
    classifiers=[
        "Programming Language :: Python :: 3",
        "License :: OSI Approved :: MIT License",
        "Operating System :: OS Independent",
    ],
)