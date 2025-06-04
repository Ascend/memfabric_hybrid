/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */

#ifndef MEM_FABRIC_HYBRID_HYBM_VERSION_H
#define MEM_FABRIC_HYBRID_HYBM_VERSION_H

/* version information */
#define VERSION_MAJOR 1
#define VERSION_MINOR 0
#define VERSION_FIX 0

/* second level marco define 'CON' to get string */
#define CONCAT(x, y, z) x.##y.##z
#define STR(x) #x
#define CONCAT2(x, y, z) CONCAT(x, y, z)
#define STR2(x) STR(x)

/* get cancat version string */
#define SM_VERSION STR2(CONCAT2(VERSION_MAJOR, VERSION_MINOR, VERSION_FIX))

#ifndef GIT_LAST_COMMIT
#define GIT_LAST_COMMIT empty
#endif

/*
 * global lib version string with build time
 */
static const char *LIB_VERSION = "library version: " SM_VERSION
                                 ", build time: " __DATE__ " " __TIME__
                                 ", commit: " STR2(GIT_LAST_COMMIT);

#endif // MEM_FABRIC_HYBRID_HYBM_VERSION_H
