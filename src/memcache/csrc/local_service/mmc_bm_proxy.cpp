/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#include "mmc_bm_proxy.h"
#include <algorithm>
#include "mmc_logger.h"
#include "mmc_smem_bm_helper.h"
#include "mmc_ptracer.h"

namespace ock {
namespace mmc {
std::map<std::string, MmcRef<MmcBmProxy>> MmcBmProxyFactory::instances_;
std::mutex MmcBmProxyFactory::instanceMutex_;

Result MmcBmProxy::InitBm(const mmc_bm_init_config_t &initConfig, const mmc_bm_create_config_t &createConfig)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (started_ && handle_ != nullptr) {
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

    config.flags = initConfig.flags;
    (void) std::copy_n(initConfig.hcomUrl.c_str(), initConfig.hcomUrl.size(), config.hcomUrl);
    MMC_RETURN_ERROR(smem_bm_init(initConfig.ipPort.c_str(), initConfig.worldSize, initConfig.deviceId, &config),
                     "Failed to init smem bm");

    bmRankId_ = smem_bm_get_rank_id();

    auto ret = InternalCreateBm(createConfig);
    if (ret != MMC_OK) {
        return ret;
    }
    void* tmpGva = nullptr;
    MMC_RETURN_ERROR(smem_bm_join(handle_, 0, &tmpGva), "Failed to join smem bm");

    gvas_[MEDIA_HBM] = smem_bm_ptr_by_mem_type(handle_, SMEM_MEM_TYPE_DEVICE, bmRankId_);
    gvas_[MEDIA_DRAM] = smem_bm_ptr_by_mem_type(handle_, SMEM_MEM_TYPE_HOST, bmRankId_);
    spaces_[MEDIA_HBM] = smem_bm_get_local_mem_size_by_mem_type(handle_, SMEM_MEM_TYPE_DEVICE);
    spaces_[MEDIA_DRAM] = smem_bm_get_local_mem_size_by_mem_type(handle_, SMEM_MEM_TYPE_HOST);
    started_ = true;

    MMC_LOG_INFO("init bm success, rank:" << bmRankId_ << ", worldSize:" << initConfig.worldSize << ", hbm{"
                                          << gvas_[MEDIA_HBM] << ", " << spaces_[MEDIA_HBM] << "}, dram{"
                                          << gvas_[MEDIA_DRAM] << "," << spaces_[MEDIA_DRAM] << "}");
    return MMC_OK;
}

Result MmcBmProxy::InternalCreateBm(const mmc_bm_create_config_t &createConfig)
{
    if (createConfig.localHBMSize > 0 && createConfig.localDRAMSize == 0) {
        mediaType_ = MEDIA_HBM;
    } else if (createConfig.localDRAMSize > 0 && createConfig.localHBMSize == 0) {
        mediaType_ = MEDIA_DRAM;
    } else {
        MMC_LOG_INFO("dram and hbm hybrid pool");
    }

    smem_bm_data_op_type opType = MmcSmemBmHelper::TransSmemBmDataOpType(createConfig);
    if (opType == SMEMB_DATA_OP_BUTT) {
        MMC_LOG_ERROR("MmcBmProxy unknown data op type " << createConfig.dataOpType);
        return MMC_ERROR;
    }
    handle_ = smem_bm_create(createConfig.id, createConfig.memberSize, opType,
        createConfig.localDRAMSize, createConfig.localHBMSize, createConfig.flags);
    if (handle_ == nullptr) {
        MMC_LOG_ERROR("Failed to create smem bm");
        return MMC_ERROR;
    }

    return MMC_OK;
}

void MmcBmProxy::DestroyBm()
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
        std::fill(gvas_, gvas_ + MEDIA_NONE, nullptr);
    }
    started_ = false;
    MMC_LOG_INFO("MmcBmProxy (" << name_ << ") is destroyed successfully");
}

MediaType MmcBmProxy::GetMediaType()
{
    // 从上到下选择写入的介质
    for (MediaType type = MEDIA_HBM; type != MEDIA_NONE;) {
        if (GetGva(type) != 0) {
            return type;
        }
        type = MoveDown(type);
    }
    MMC_LOG_ERROR("MmcBmProxy GetMediaType unknown media type");
    return MEDIA_NONE;
}

Result MmcBmProxy::Put(uint64_t srcBmAddr, uint64_t dstBmAddr, uint64_t size, smem_bm_copy_type type)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (handle_ == nullptr) {
        MMC_LOG_ERROR("Failed to put data to smem bm, handle is null");
        return MMC_ERROR;
    }
    TP_TRACE_BEGIN(TP_SMEM_BM_PUT);
    auto ret = smem_bm_copy(handle_, (void*)srcBmAddr, (void*)dstBmAddr, size, type, 0);
    TP_TRACE_END(TP_SMEM_BM_PUT, ret);
    return ret;
}

Result MmcBmProxy::Put(const mmc_buffer* buf, uint64_t bmAddr, uint64_t size)
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
        TP_TRACE_BEGIN(TP_SMEM_BM_PUT);
        auto ret =
            smem_bm_copy(handle_, (void*)(buf->addr + buf->oneDim.offset), (void*)bmAddr, buf->oneDim.len,
                         type, ASYNC_COPY_FLAG);
        TP_TRACE_END(TP_SMEM_BM_PUT, ret);
        return ret;
    } else if (buf->dimType == 1) {
        if (buf->twoDim.width * buf->twoDim.layerNum > size) {
            MMC_LOG_ERROR("Failed to put data to smem bm, buf size : " << buf->twoDim.width * buf->twoDim.layerNum
                << " is larger than bm block size : " << size);
            return MMC_ERROR;
        }
        if (buf->twoDim.dpitch >= buf->twoDim.width) {
            TP_TRACE_BEGIN(TP_SMEM_BM_PUT_2D);
            auto ret = smem_bm_copy_2d(handle_, (void*)(buf->addr + buf->twoDim.dpitch * buf->twoDim.layerOffset),
                                       buf->twoDim.dpitch, (void*)bmAddr, buf->twoDim.width, buf->twoDim.width,
                                       buf->twoDim.layerNum, type, 0);
            TP_TRACE_END(TP_SMEM_BM_PUT_2D, ret);
            return ret;
        } else {
            MMC_LOG_ERROR("MmcBmProxy Put dpitch (" << buf->twoDim.dpitch << ") should be larger or equal to width ("
                          << buf->twoDim.width << ")");
            return MMC_ERROR;
        }
    } else {
        MMC_LOG_ERROR("MmcBmProxy Put unknown buf type");
        return MMC_ERROR;
    }
}

Result MmcBmProxy::Get(const mmc_buffer* buf, uint64_t bmAddr, uint64_t size)
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
    smem_bm_copy_type type = buf->type == 0 ? SMEMB_COPY_G2H : SMEMB_COPY_G2L;
    if (buf->dimType == 0) {
        if (buf->oneDim.len > size) {
            MMC_LOG_ERROR("Failed to get data to smem bm, buf length: " << buf->oneDim.len
                          << " not equal data length: " << size);
            return MMC_ERROR;
        }
        TP_TRACE_BEGIN(TP_SMEM_BM_GET);
        auto ret =
            smem_bm_copy(handle_, (void*)bmAddr, (void*)(buf->addr + buf->oneDim.offset), buf->oneDim.len,
                         type, ASYNC_COPY_FLAG);
        TP_TRACE_END(TP_SMEM_BM_GET, ret);
        return ret;
    } else if (buf->dimType == 1) {
        uint64_t length = buf->twoDim.width * buf->twoDim.layerNum;
        if (length != size) {
            MMC_LOG_ERROR("Failed to get data to smem bm, buf length: " << length
                          << " not equal data length: " << size);
            return MMC_ERROR;
        }
        if (buf->twoDim.dpitch == buf->twoDim.width) {
            TP_TRACE_BEGIN(TP_SMEM_BM_GET);
            auto ret =
                smem_bm_copy(handle_, (void*)bmAddr, (void*)(buf->addr + buf->twoDim.dpitch * buf->twoDim.layerOffset),
                             buf->twoDim.width * buf->twoDim.layerNum, type, ASYNC_COPY_FLAG);
            TP_TRACE_END(TP_SMEM_BM_GET, ret);
            return ret;
        } else if (buf->twoDim.dpitch > buf->twoDim.width) {
            TP_TRACE_BEGIN(TP_SMEM_BM_GET_2D);
            auto ret = smem_bm_copy_2d(handle_, (void*)bmAddr, buf->twoDim.width,
                                       (void*)(buf->addr + buf->twoDim.dpitch * buf->twoDim.layerOffset),
                                       buf->twoDim.dpitch, buf->twoDim.width, buf->twoDim.layerNum, type, 0);
            TP_TRACE_END(TP_SMEM_BM_GET_2D, ret);
            return ret;
        } else {
            MMC_LOG_ERROR("MmcBmProxy Get dpitch should be larger or equal to width");
            return MMC_ERROR;
        }
    } else {
        MMC_LOG_ERROR("MmcBmProxy Get unknown buf type");
        return MMC_ERROR;
    }
}

Result MmcBmProxy::Put(const MmcBufferArray& bufArr, const MmcMemBlobDesc& blob)
{
    if (handle_ == nullptr) {
        MMC_LOG_ERROR("Failed to get data to smem bm, handle is null");
        return MMC_ERROR;
    }

    if (bufArr.TotalSize() != blob.size_) {
        MMC_LOG_ERROR("Failed to put data to smem bm, total buffer size : " << bufArr.TotalSize()
                << " is not equal to bm block size: " << blob.size_);
        return MMC_ERROR;
    }

    size_t shift = 0;
    for (const auto& buffer : bufArr.Buffers()) {
        MMC_RETURN_ERROR(Put(&buffer, blob.gva_ + shift, blob.size_ - shift), "failed put data to smem bm");
        shift += MmcBufSize(buffer);
    }
    return MMC_OK;
}

Result MmcBmProxy::Get(const MmcBufferArray& bufArr, const MmcMemBlobDesc& blob)
{
    if (handle_ == nullptr) {
        MMC_LOG_ERROR("Failed to get data to smem bm, handle is null");
        return MMC_ERROR;
    }

    if (bufArr.TotalSize() != blob.size_) {
        MMC_LOG_ERROR("Failed to get data from smem bm, total buffer size : " << bufArr.TotalSize()
                << " is not equal to bm block size: " << blob.size_);
        return MMC_ERROR;
    }

    size_t shift = 0;
    for (const auto &buffer : bufArr.Buffers()) {
        MMC_RETURN_ERROR(Get(&buffer, blob.gva_ + shift, blob.size_ - shift), "Failed to get data from smem bm");
        shift += MmcBufSize(buffer);
    }
    return MMC_OK;
}

Result MmcBmProxy::BatchPut(const MmcBufferArray& bufArr, const MmcMemBlobDesc& blob)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (handle_ == nullptr) {
        MMC_LOG_ERROR("Failed to get data to smem bm, handle is null");
        return MMC_ERROR;
    }
    if (bufArr.TotalSize() != blob.size_) {
        MMC_LOG_ERROR("Failed to get data from smem bm, total buffer size : " << bufArr.TotalSize()
            << " is not equal to bm block size: " << blob.size_);
        return MMC_ERROR;
    }
    size_t shift = 0;
    uint32_t count = bufArr.Buffers().size();
    const void *sources[count];
    void *destinations[count];
    uint32_t dataSizes[count];
    smem_bm_copy_type type = bufArr.Buffers()[0].type == 0 ? SMEMB_COPY_H2G : SMEMB_COPY_L2G;
    for (size_t i = 0; i < count; ++i) {
        auto buf = &bufArr.Buffers()[i];
        if (buf->dimType == 0) {
            sources[i] = reinterpret_cast<void *>(buf->addr + buf->oneDim.offset);
        } else if (buf->dimType == 1) {
            sources[i] = reinterpret_cast<void *>(buf->addr + buf->twoDim.dpitch * buf->twoDim.layerOffset);
        } else {
            MMC_LOG_ERROR("Not support type");
            return MMC_ERROR;
        }
        destinations[i] = reinterpret_cast<void *>(blob.gva_ + shift);
        dataSizes[i] = buf->oneDim.len;
        shift += MmcBufSize(*buf);
    }
    smem_batch_copy_params batch_params = {sources, destinations, dataSizes, count};
    return smem_bm_copy_batch(handle_, &batch_params, type, 0);
}

Result MmcBmProxy::BatchGet(const MmcBufferArray& bufArr, const MmcMemBlobDesc& blob)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (handle_ == nullptr) {
        MMC_LOG_ERROR("Failed to get data to smem bm, handle is null");
        return MMC_ERROR;
    }
    if (bufArr.TotalSize() != blob.size_) {
        MMC_LOG_ERROR("Failed to get data from smem bm, total buffer size : " << bufArr.TotalSize()
            << " is not equal to bm block size: " << blob.size_);
        return MMC_ERROR;
    }
    size_t shift = 0;
    uint32_t count = bufArr.Buffers().size();
    const void *sources[count];
    void *destinations[count];
    uint32_t dataSizes[count];
    smem_bm_copy_type type = bufArr.Buffers()[0].type == 0 ? SMEMB_COPY_G2H : SMEMB_COPY_G2L;
    for (size_t i = 0; i < count; ++i) {
        auto buf = &bufArr.Buffers()[i];
        if (buf->dimType == 0) {
            destinations[i] = reinterpret_cast<void *>(buf->addr + buf->oneDim.offset);
        } else if (buf->dimType == 1) {
            destinations[i] = reinterpret_cast<void *>(buf->addr + buf->twoDim.dpitch * buf->twoDim.layerOffset);
        } else {
            MMC_LOG_ERROR("Not support type");
            return MMC_ERROR;
        }
        sources[i] = reinterpret_cast<void *>(blob.gva_ + shift);
        dataSizes[i] = buf->oneDim.len;
        shift += MmcBufSize(*buf);
    }
    smem_batch_copy_params batch_params = {sources, destinations, dataSizes, count};
    return smem_bm_copy_batch(handle_, &batch_params, type, 0);
}

Result MmcBmProxy::CopyWait()
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto ret = smem_bm_wait(handle_);
    if (ret != MMC_OK) {
        MMC_LOG_ERROR("Failed to wait copy task ret:" << ret);
    }
    return ret;
}

}
}