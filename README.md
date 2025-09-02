# memfabric_hybrid

### Description
MemFabric Hybrid is a memory pool facility for Huawei Ascend Npu Server and Super Pod, providing easy way to build memory pool cross nodes include HBM and DRAM. It provides a parallel programing interface to user and creates a global address space for data that span the memory for multiple NPUs and can be accessed with NPU-initiated data operations using MTE, RoCE, SDMA, also provides data copy operations initiated by CPU.

### Software Architecture
Software architecture description

### How build

To build binary, it doesn't reply on CANN and HDK.

##### 1. Download the code using git and its dependent submodule
```
git clone git@gitee.com:ascend/memfabric_hybrid.git

cd memfabric_hybrid

git submodule init

git submodule update --recursive

```

##### 2. Build
**Method 1: Build manually**
```
cd memfabric_hybrid

mkdir build

cd build 

cmake ..

make install -j4

ll ../output
```

**Method 2: Use the one key build script**
```
cd memfabric_hybrid

bash script/build.sh

tree ../output
```

**Method 3: Use the one key build&package script**
```
cd memfabric_hybrid

bash script/build_and_pack_run.sh
```
The output is a .run package in ../output

##### 3. Install
```shell
cd output

chmod +x mxc-memfabric_hybrid-1.0.0_linux_aarch64.run

./mxc-memfabric_hybrid-1.0.0_linux_aarch64.run --install
```

##### 4. Run
1. Start MetaService

- Method 1: Start from executable file
```shell
source /usr/local/mxc/memfabric_hybrid/set_env.sh
export MMC_META_CONFIG_PATH=/usr/local/mxc/memfabric_hybrid/latest/config/mmc-meta.conf

/usr/local/mxc/memfabric_hybrid/latest/aarch64-linux/bin/mmc_meta_service
```

- Method 2: Start using a [Python script](test/python/memcache/test_mmc_meta_service.py)
```shell
export MMC_META_CONFIG_PATH=/usr/local/mxc/memfabric_hybrid/latest/config/mmc-meta.conf

python3 test_mmc_meta_service.py
```

2. Start LocalService & Client in a demo [Python program](test/python/memcache/test_mmc_demo.py)
```shell
export MMC_LOCAL_CONFIG_PATH=/usr/local/mxc/memfabric_hybrid/latest/config/mmc-local.conf

python3 test_mmc_demo.py
```

### 5. Detailed Documentations
- How to use: [Introduction for Use](doc/zh/使用说明.md)
- API Doc: [MMC_API](doc/zh/MMC_API.md)