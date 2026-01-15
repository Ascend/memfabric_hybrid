/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * MemFabric_Hybrid is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
*/

#ifndef MEM_FABRIC_HYBRID_SMEM_VERSION_H
#define MEM_FABRIC_HYBRID_SMEM_VERSION_H

#ifdef __cplusplus
extern "C" {
#endif
/* second level marco define 'CON' to get string */
#define CONCAT(x, y, z)  x.##y.##z
#define STR(x)           #x
#define CONCAT2(x, y, z) CONCAT(x, y, z)
#define STR2(x)          STR(x)

/* get cancat version string */
#define SM_VERSION STR2(CONCAT2(VERSION_MAJOR, VERSION_MINOR, VERSION_FIX))

#ifndef GIT_LAST_COMMIT
#define GIT_LAST_COMMIT empty
#endif

/*
 * global lib version string with build time
 */
static const char *LIB_VERSION =
    "library version: " SM_VERSION ", build time: " __DATE__ " " __TIME__ ", commit: " STR2(GIT_LAST_COMMIT);

#ifdef __cplusplus
}
#endif

#endif // MEM_FABRIC_HYBRID_SMEM_VERSION_H
