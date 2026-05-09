/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ZBAL is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#include "zbal_mem_allocator.h"
#include "zbal_pytorch_c10_dma.h"
#include "zbal_sma.h"
#include "zbal_sma_config.h"
#include "zbal_defines.h"
#include <c10/util/flat_hash_map.h>

bool gGVASpaceInited = false;
ska::flat_hash_set<void *> gDmaBlocks;

#ifdef __cplusplus
extern "C" {
#endif

ZBAL_API int32_t zbal_sma_init(zbal_allocator_options_t *options, int32_t flags)
{
    if (zbal::sma::SMAConfig::use_vmm_for_static_memory()) {
        sma_init_heap(options->myGva, options->size);
    } else {
        if (!zbal::sma::SMAConfig::use_sma_allocator()) {
            std::cout << "[ZBAL] using dma as allocator!" << std::endl;
            dma_init_heap(options->myGva, options->size);
        } else {
            std::cout << "[ZBAL] using sma as allocator!" << std::endl;
            sma_init_heap(options->myGva, options->size);
        }
    }
    gGVASpaceInited = true;
    return zbal::ZResultErrorCode::Z_OK;
}

ZBAL_API void zbal_sma_uninit(int32_t flags)
{
    // TODO: get trigger time correctly, maybe callback
    return;
}

ZBAL_API void zbal_pluggable_init(int32_t device_count)
{
    if (zbal::sma::SMAConfig::use_vmm_for_static_memory()) {
        dma_init(device_count);
        sma_init(device_count);
    } else {
        if (!zbal::sma::SMAConfig::use_sma_allocator()) {
            dma_init(device_count);
        } else {
            sma_init(device_count);
        }
    }
}

ZBAL_API void *zbal_pluggable_malloc(size_t size, int32_t device, aclrtStream stream)
{
    ZBAL_ASSERT_S(zbal::sma::SMAConfig::use_vmm_for_static_memory() || gGVASpaceInited,
                  "gva space not inited, can not allocate memory.", zbal::Z_ERROR);
    ZBAL_ASSERT_S(zbal::sma::SMAConfig::use_vmm_for_static_memory() || !c10_npu::dma::checkConfigExpandableSegments(),
                  "expandable_segments can only be used together with use_vmm_for_static_memory", zbal::Z_ERROR);
    if (zbal::sma::SMAConfig::use_vmm_for_static_memory()) {
        if (!gGVASpaceInited) {
            auto ptr = dma_malloc(size, device, stream);
            gDmaBlocks.insert(ptr);
            return ptr;
        } else {
            return sma_malloc(size, device, stream);
        }
    } else {
        if (!zbal::sma::SMAConfig::use_sma_allocator()) {
            return dma_malloc(size, device, stream);
        } else {
            return sma_malloc(size, device, stream);
        }
    }
}

ZBAL_API void zbal_pluggable_free(void *ptr, size_t size, int32_t device, aclrtStream stream)
{
    if (zbal::sma::SMAConfig::use_vmm_for_static_memory()) {
        if (gDmaBlocks.count(ptr)) {
            gDmaBlocks.erase(ptr);
            dma_free(ptr, size, device, stream);
        } else {
            sma_free(ptr, size, device, stream);
        }
    } else {
        if (!zbal::sma::SMAConfig::use_sma_allocator()) {
            dma_free(ptr, size, device, stream);
        } else {
            sma_free(ptr, size, device, stream);
        }
    }
}

ZBAL_API void zbal_pluggable_empty_cache(bool check_error)
{
    if (zbal::sma::SMAConfig::use_vmm_for_static_memory()) {
        dma_empty_cache(check_error);
        sma_empty_cache(check_error);
    } else {
        if (!zbal::sma::SMAConfig::use_sma_allocator()) {
            dma_empty_cache(check_error);
        } else {
            sma_empty_cache(check_error);
        }
    }
}

ZBAL_API void zbal_pluggable_record_stream(void *ptr, c10_npu::NPUStream stream)
{
    if (zbal::sma::SMAConfig::use_vmm_for_static_memory()) {
        if (gDmaBlocks.count(ptr)) {
            dma_record_stream(ptr, stream);
        } else {
            sma_record_stream(ptr, stream);
        }
    } else {
        if (!zbal::sma::SMAConfig::use_sma_allocator()) {
            dma_record_stream(ptr, stream);
        } else {
            sma_record_stream(ptr, stream);
        }
    }
}

ZBAL_API void zbal_pluggable_erase_stream(void *ptr, c10_npu::NPUStream stream)
{
    if (zbal::sma::SMAConfig::use_vmm_for_static_memory()) {
        if (gDmaBlocks.count(ptr)) {
            dma_erase_stream(ptr, stream);
        } else {
            sma_erase_stream(ptr, stream);
        }
    } else {
        if (!zbal::sma::SMAConfig::use_sma_allocator()) {
            dma_erase_stream(ptr, stream);
        } else {
            sma_erase_stream(ptr, stream);
        }
    }
}

// deprecated
ZBAL_API void *zbal_get_symm_base_addr()
{
    ZBAL_LOG_WARN("base addr no longer stands for meta if inited from bootstrap, will be deprecated soon");
    if (zbal::sma::SMAConfig::use_vmm_for_static_memory()) {
        return sma_get_base_addr();
    } else {
        if (!zbal::sma::SMAConfig::use_sma_allocator()) {
            return dma_get_base_addr();
        } else {
            return sma_get_base_addr();
        }
    }
}

ZBAL_API void zbal_pluggable_begin_allocate_to_pool(int device, c10_npu::MempoolId_t mempool_id,
                                                    std::function<bool(aclrtStream)> filter)
{
    if (zbal::sma::SMAConfig::use_vmm_for_static_memory()) {
        if (!gGVASpaceInited) {
            dma_begin_allocate_to_pool(device, mempool_id, filter);
        } else {
            sma_begin_allocate_to_pool(device, mempool_id, filter);
        }
    } else {
        if (!zbal::sma::SMAConfig::use_sma_allocator()) {
            dma_begin_allocate_to_pool(device, mempool_id, filter);
        } else {
            sma_begin_allocate_to_pool(device, mempool_id, filter);
        }
    }
}

ZBAL_API void zbal_pluggable_end_allocate_to_pool(int device, c10_npu::MempoolId_t mempool_id)
{
    if (zbal::sma::SMAConfig::use_vmm_for_static_memory()) {
        if (!gGVASpaceInited) {
            dma_end_allocate_to_pool(device, mempool_id);
        } else {
            sma_end_allocate_to_pool(device, mempool_id);
        }
    } else {
        if (!zbal::sma::SMAConfig::use_sma_allocator()) {
            dma_end_allocate_to_pool(device, mempool_id);
        } else {
            sma_end_allocate_to_pool(device, mempool_id);
        }
    }
}

ZBAL_API void zbal_pluggable_release_pool(int device, c10_npu::MempoolId_t mempool_id)
{
    if (zbal::sma::SMAConfig::use_vmm_for_static_memory()) {
        if (!gGVASpaceInited) {
            dma_release_pool(device, mempool_id);
        } else {
            sma_release_pool(device, mempool_id);
        }
    } else {
        if (!zbal::sma::SMAConfig::use_sma_allocator()) {
            dma_release_pool(device, mempool_id);
        } else {
            sma_release_pool(device, mempool_id);
        }
    }
}

ZBAL_API void zbal_simulate_init(int64_t addr, int64_t size)
{
    ZBAL_LOG_ERROR("simulate init is only applied for allocator replay, any action on write/read memory will cause "
                   "unexpected error!");
    ZBAL_ASSERT_S(!zbal::sma::SMAConfig::use_vmm_for_static_memory(), "mix allocator do not support simulate",
                  zbal::Z_ERROR);
    if (!zbal::sma::SMAConfig::use_sma_allocator()) {
        dma_init_heap(reinterpret_cast<void *>(addr), size);
    } else {
        sma_init_heap(reinterpret_cast<void *>(addr), size);
    }
    gGVASpaceInited = true;
    return;
}

ZBAL_API c10_npu::NPUCachingAllocator::DeviceStats zbal_pluggable_get_device_stats(int device)
{
    ZBAL_ASSERT_S(!zbal::sma::SMAConfig::use_vmm_for_static_memory(), "mix allocator do not support get_device_stats",
                  zbal::Z_ERROR);
    if (!zbal::sma::SMAConfig::use_sma_allocator()) {
        return dma_get_device_stats(device);
    } else {
        return sma_get_device_stats(device);
    }
}

#ifdef __cplusplus
}
#endif
