/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Embricks is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#include "emb_flash_hashmap_persist.h"

namespace ock {
namespace emb {
namespace hashmap {
Result FlashHashmapPersist::PersistMutable(const std::string &filepath, const FlashHashmapPtr &map)
{
    return EM_OK;
}

Result FlashHashmapPersist::UnPersistMutable(const std::string &filepath, const FlashHashmapPtr &map)
{
    return EM_OK;
}

Result FlashHashmapPersist::UnPersistReadonly(const std::string &filepath, const ReadonlyFlashHashmapPtr &map)
{
    return EM_OK;
}
} // namespace hashmap
} // namespace emb
} // namespace ock
