# memfabric_hybrid

#### Description
MemFabric Hybrid is a memory pool facility for Huawei Ascend Npu Server and Super Pod, providing easy way to build memory pool cross nodes include HBM and DRAM. It provides a parallel programing interface to user and creates a global address space for data that span the memory for multiple NPUs and can be accessed with NPU-initiated data operations using MTE, RoCE, SDMA, also provides data copy operations initiated by CPU.

#### Software Architecture
Software architecture description

#### How build

To build binary, it doesn't reply on CANN and HDK.

1. Download the code using git and its dependent submodule
```
git clone git@gitee.com:ascend/memfabric_hybrid.git

cd memfabric_hybrid

git submodule init

git submodule update --recursive

```

2. Build the binary
```
cd memfabric_hybrid

mkdir build

cd build 

cmake ..

make install -j4

ll ../output/smem
```

3 run example
```

```