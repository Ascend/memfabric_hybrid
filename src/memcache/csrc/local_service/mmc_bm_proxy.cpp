/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#include "mmc_bm_proxy.h"
#include "mmc_logger.h"

namespace ock {
namespace mmc {
std::map<std::string, MmcRef<MmcBmProxy>> MmcBmProxyFactory::instances_;
std::mutex MmcBmProxyFactory::instanceMutex_;

Result MmcBmProxy::InitBm(const mmc_bm_init_config_t &initConfig, const mmc_bm_create_config_t &createConfig)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (started_) {
        MMC_LOG_INFO("MmcBmProxy " << name_ << " already init");
        return MMC_OK;
    }
    if (handle_ != nullptr) {
        MMC_LOG_ERROR("Bm proxy has been initialized");
        return MMC_ERROR;
    }

    auto ret = smem_init(0);
    if (ret != 0) {
        MMC_LOG_ERROR("Failed to init smem");
        return ret;
    }

    smem_bm_config_t config;
    ret = smem_bm_config_init(&config);
    if (ret != 0) {
        MMC_LOG_ERROR("Failed to init smem bm config");
        return ret;
    }

    if (initConfig.autoRanking == 1) {
        config.autoRanking = true;
    } else {
        config.rankId = initConfig.rankId;
    }

    ret = smem_bm_init(initConfig.ipPort.c_str(), initConfig.worldSize, initConfig.deviceId, &config);
    if (ret != 0) {
        MMC_LOG_ERROR("Failed to init smem bm");
        return ret;
    }

    smem_bm_data_op_type opType = SMEMB_DATA_OP_SDMA;
    if (createConfig.dataOpType == "sdma") {
        opType = SMEMB_DATA_OP_SDMA;
    } else if (createConfig.dataOpType == "roce") {
        opType = SMEMB_DATA_OP_ROCE;
    }
    handle_ = smem_bm_create(createConfig.id, createConfig.memberSize, opType,
        createConfig.localDRAMSize, createConfig.localHBMSize, createConfig.flags);
    if (handle_ == nullptr) {
        MMC_LOG_ERROR("Failed to create smem bm");
        return MMC_ERROR;
    }

    ret = smem_bm_join(handle_, 0, &gva_);
    if (ret != 0) {
        MMC_LOG_ERROR("Failed to join smem bm");
        return ret;
    }
    started_ = true;
    return MMC_OK;
}

void MmcBmProxy::DestoryBm()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!started_) {
        MMC_LOG_ERROR("MmcBmProxy (" << name_ << ") is not init");
        return ;
    }

    if (handle_ != nullptr) {
        smem_bm_destroy(handle_);
        smem_bm_uninit(0);
        handle_ = nullptr;
        gva_ = nullptr;
    }
}
// todo size校验
Result MmcBmProxy::Put(mmc_buffer *buf, uint64_t bmAddr, uint64_t size)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (handle_ == nullptr) {
        MMC_LOG_ERROR("Failed to put data to smem bm, handle is null");
        return MMC_ERROR;
    }
    if (buf == nullptr) {
        MMC_LOG_ERROR("Failed to put data to smem bm, buf is null");
        return MMC_ERROR;
    }
    if (buf->type == 0) {
        if (buf->dram.len > size) {
            MMC_LOG_ERROR("Failed to put data to smem bm, buf size : " << buf->dram.len
                << " is larger than bm block size : " << size);
            return MMC_ERROR;
        }
        return smem_bm_copy(handle_, (void *)(buf->addr + buf->dram.offset), (void *)bmAddr, buf->dram.len,
            SMEMB_COPY_H2G, 0);
    } else if (buf->type == 1) {
        if (buf->hbm.width * buf->hbm.layerNum > size) {
            MMC_LOG_ERROR("Failed to put data to smem bm, buf size : " << buf->hbm.width * buf->hbm.layerNum
                << " is larger than bm block size : " << size);
            return MMC_ERROR;
        }
        if (buf->hbm.dpitch == buf->hbm.width) {
            return smem_bm_copy(handle_, (void *)(buf->addr + buf->hbm.dpitch * buf->hbm.layerOffset), (void *)bmAddr,
                buf->hbm.width * buf->hbm.layerNum, SMEMB_COPY_L2G, 0);
        } else if (buf->hbm.dpitch > buf->hbm.width) {
            return smem_bm_copy_2d(handle_, (void *)(buf->addr + buf->hbm.dpitch * buf->hbm.layerOffset),
                buf->hbm.dpitch, (void *)bmAddr, buf->hbm.width, buf->hbm.width, buf->hbm.layerNum, SMEMB_COPY_L2G, 0);
        } else {
            MMC_LOG_ERROR("MmcBmProxy Put dpitch should be larger or equal to width");
            return MMC_ERROR;
        }
    } else {
        MMC_LOG_ERROR("MmcBmProxy Put unknown buf type");
        return MMC_ERROR;
    }
}

Result MmcBmProxy::Get(mmc_buffer *buf, uint64_t bmAddr)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (handle_ == nullptr) {
        MMC_LOG_ERROR("Failed to get data to smem bm, handle is null");
        return MMC_ERROR;
    }
    if (buf == nullptr) {
        MMC_LOG_ERROR("Failed to get data to smem bm, buf is null");
        return MMC_ERROR;
    }
    if (buf->type == 0) {
        return smem_bm_copy(handle_, (void *)bmAddr, (void *)(buf->addr + buf->dram.offset), buf->dram.len,
            SMEMB_COPY_G2H, 0);
    } else if (buf->type == 1) {
        if (buf->hbm.dpitch == buf->hbm.width) {
            return smem_bm_copy(handle_, (void *)bmAddr, (void *)(buf->addr + buf->hbm.dpitch * buf->hbm.layerOffset),
                buf->hbm.width * buf->hbm.layerNum, SMEMB_COPY_G2L, 0);
        } else if (buf->hbm.dpitch > buf->hbm.width) {
            return smem_bm_copy_2d(handle_, (void *)bmAddr, buf->hbm.width,
                (void *)(buf->addr + buf->hbm.dpitch * buf->hbm.layerOffset), buf->hbm.dpitch,
                buf->hbm.width, buf->hbm.layerNum, SMEMB_COPY_G2L, 0);
        } else {
            MMC_LOG_ERROR("MmcBmProxy Get dpitch should be larger or equal to width");
            return MMC_ERROR;
        }
    } else {
        MMC_LOG_ERROR("MmcBmProxy Get unknown buf type");
        return MMC_ERROR;
    }
}
}
}