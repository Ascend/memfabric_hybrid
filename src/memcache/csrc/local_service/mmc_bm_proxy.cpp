/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#include "mmc_bm_proxy.h"
#include <algorithm>
#include "mmc_logger.h"

namespace ock {
namespace mmc {
std::map<std::string, MmcRef<MmcBmProxy>> MmcBmProxyFactory::instances_;
std::mutex MmcBmProxyFactory::instanceMutex_;

Result MmcBmProxy::InitBm(const mmc_bm_init_config_t &initConfig, const mmc_bm_create_config_t &createConfig)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (started_ || handle_ != nullptr) {
        MMC_LOG_INFO("MmcBmProxy " << name_ << " already init");
        return MMC_OK;
    }
    MMC_RETURN_ERROR(smem_set_log_level(initConfig.logLevel), "Failed to set smem bm log level");
    if (initConfig.logFunc != nullptr) {
        MMC_RETURN_ERROR(smem_set_extern_logger(initConfig.logFunc), "Failed to set smem bm extern logger");
    }

    MMC_RETURN_ERROR(smem_init(0), "Failed to init smem");

    smem_bm_config_t config;
    MMC_RETURN_ERROR(smem_bm_config_init(&config), "Failed to init smem bm config");
    if (initConfig.autoRanking == 1) {
        config.autoRanking = true;
    } else {
        config.rankId = initConfig.rankId;
    }

    (void) std::copy_n(initConfig.hcomUrl.c_str(), initConfig.hcomUrl.size(), config.hcomUrl);
    MMC_RETURN_ERROR(smem_bm_init(initConfig.ipPort.c_str(), initConfig.worldSize, initConfig.deviceId, &config),
                     "Failed to init smem bm");

    bmRankId_ = (initConfig.autoRanking == 1) ? smem_bm_get_rank_id() : initConfig.rankId;

    smem_bm_data_op_type opType = SMEMB_DATA_OP_SDMA;
    if (createConfig.dataOpType == "sdma") {
        opType = SMEMB_DATA_OP_SDMA;
    } else if (createConfig.dataOpType == "roce") {
        opType = SMEMB_DATA_OP_ROCE;
    } else {
        MMC_LOG_ERROR("MmcBmProxy unknown data op type " << createConfig.dataOpType);
        return MMC_ERROR;
    }
    handle_ = smem_bm_create(createConfig.id, createConfig.memberSize, opType,
        createConfig.localDRAMSize, createConfig.localHBMSize, createConfig.flags);
    if (handle_ == nullptr) {
        MMC_LOG_ERROR("Failed to create smem bm");
        return MMC_ERROR;
    }

    MMC_RETURN_ERROR(smem_bm_join(handle_, 0, &gva_), "Failed to join smem bm");
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
        smem_uninit();
        handle_ = nullptr;
        gva_ = nullptr;
    }
    started_ = false;
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
    smem_bm_copy_type type = buf->type == 0 ? SMEMB_COPY_H2G : SMEMB_COPY_L2G;
    if (buf->dimType == 0) {
        if (buf->oneDim.len > size) {
            MMC_LOG_ERROR("Failed to put data to smem bm, buf size : " << buf->oneDim.len
                << " is larger than bm block size : " << size);
            return MMC_ERROR;
        }
        return smem_bm_copy(handle_, (void *)(buf->addr + buf->oneDim.offset), (void *)bmAddr, buf->oneDim.len,
                            type, 0);
    } else if (buf->dimType == 1) {
        if (buf->twoDim.width * buf->twoDim.layerNum > size) {
            MMC_LOG_ERROR("Failed to put data to smem bm, buf size : " << buf->twoDim.width * buf->twoDim.layerNum
                << " is larger than bm block size : " << size);
            return MMC_ERROR;
        }
        if (buf->twoDim.dpitch > buf->twoDim.width) {
            return smem_bm_copy_2d(handle_, (void *)(buf->addr + buf->twoDim.dpitch * buf->twoDim.layerOffset),
                                   buf->twoDim.dpitch, (void *)bmAddr, buf->twoDim.width, buf->twoDim.width, buf->twoDim.layerNum, type, 0);
        } else {
            MMC_LOG_ERROR("MmcBmProxy Put dpitch should be larger or equal to width");
            return MMC_ERROR;
        }
    } else {
        MMC_LOG_ERROR("MmcBmProxy Put unknown buf type");
        return MMC_ERROR;
    }
}

Result MmcBmProxy::Get(mmc_buffer *buf, uint64_t bmAddr, uint64_t size)
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
    if (buf->dimType == 0) {
        if (buf->oneDim.len != size) {
            MMC_LOG_ERROR("Failed to get data to smem bm, buf length: " << buf->oneDim.len
                          << " not equal data length: " << size);
            return MMC_ERROR;
        }
        return smem_bm_copy(handle_, (void *)bmAddr, (void *)(buf->addr + buf->oneDim.offset), buf->oneDim.len,
                            SMEMB_COPY_G2H, 0);
    } else if (buf->dimType == 1) {
        uint64_t length = buf->twoDim.width * buf->twoDim.layerNum;
        if (length != size) {
            MMC_LOG_ERROR("Failed to get data to smem bm, buf length: " << length
                          << " not equal data length: " << size);
            return MMC_ERROR;
        }
        if (buf->twoDim.dpitch == buf->twoDim.width) {
            return smem_bm_copy(handle_, (void *)bmAddr, (void *)(buf->addr + buf->twoDim.dpitch * buf->twoDim.layerOffset),
                                buf->twoDim.width * buf->twoDim.layerNum, SMEMB_COPY_G2L, 0);
        } else if (buf->twoDim.dpitch > buf->twoDim.width) {
            return smem_bm_copy_2d(handle_, (void *)bmAddr, buf->twoDim.width,
                                   (void *)(buf->addr + buf->twoDim.dpitch * buf->twoDim.layerOffset), buf->twoDim.dpitch,
                                   buf->twoDim.width, buf->twoDim.layerNum, SMEMB_COPY_G2L, 0);
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