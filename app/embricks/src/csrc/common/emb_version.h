/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Embricks is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#ifndef MEMFABRIC_HYBRID_EMB_VERSION_H
#define MEMFABRIC_HYBRID_EMB_VERSION_H

/* version information, these variables are read in cmake from VERSION file */
#ifndef EMB_VERSION_MAJOR
#define EMB_VERSION_MAJOR 0
#define EMB_VERSION_MINOR 0
#define EMB_VERSION_FIX   0
#endif

/* second level marco define 'CONCAT' to get string */
#define CONCAT(x, y, z)  x.##y.##z
#define STR(x)           #x
#define CONCAT2(x, y, z) CONCAT(x, y, z)
#define STR2(x)          STR(x)

/* get cancat version string */
#define LIB_VERSION STR2(CONCAT2(EMB_VERSION_MAJOR, EMB_VERSION_MINOR, EMB_VERSION_FIX))

#ifndef GIT_LAST_COMMIT
#define GIT_LAST_COMMIT empty
#endif

/*
 * global lib version string with build time
 */
[[maybe_unused]] static const char *LIB_VERSION_FULL =
    "library version: " LIB_VERSION ", build time: " __DATE__ " " __TIME__ ", commit: " STR2(GIT_LAST_COMMIT);

#endif // MEMFABRIC_HYBRID_EMB_VERSION_H
