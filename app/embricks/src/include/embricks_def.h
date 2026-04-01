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
#ifndef MEMFABRIC_HYBRID_EMBRICKS_DEF_H
#define MEMFABRIC_HYBRID_EMBRICKS_DEF_H

#include <stdint.h>
#include <stdio.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EMB_TABLE_NAME_MAX  32   /* max length of table name */
#define EMB_TABLE_TAG_MAX   128  /* max length of table tags */
#define EMB_SERVICE_URL_MAX 4096 /* max length of service url */

/* definitions for embedding table */
typedef enum {
    EMB_DTYPE_FP8 = 0,
    EMB_DTYPE_FP16,
    EMB_DTYPE_FP32,

    EMB_DTYPE_BUTT
} emb_table_dtype_t;

typedef enum {
    EMB_TABLE_STORAGE_TYPE_HOST_MEM = 0, /* store in host dram */
    EMB_TABLE_STORAGE_TYPE_HOST_DISK,    /* store in host disk */

    EMB_TABLE_STORAGE_TYPE_BUTT
} emb_table_storage_type_t;

typedef enum {
    EMB_TABLE_ACCESS_TYPE_READONLY = 0, /* readonly */
    EMB_TABLE_ACCESS_TYPE_READ_WRITE,   /* write and readable */

    EMB_TABLE_ACCESS_TYPE_BUTT
} emb_table_access_type_t;

typedef struct {
    char name[EMB_TABLE_NAME_MAX];        /* name of the table */
    emb_table_dtype_t dataType;           /* data of the table */
    emb_table_storage_type_t storageType; /* storage type of the table */
    uint64_t columnCount;                 /* count of columns */
    uint64_t rawCount;                    /* count of rows */
    char tags[EMB_TABLE_TAG_MAX];         /* tag of the table, separated by comma */
} emb_table_options_t;

/* definitions for embedding service */
typedef enum {
    EMB_SERVICE_TYPE_SERVER = 0,
    EMB_SERVICE_TYPE_PROXY,
    EMB_SERVICE_TYPE_LOCAL,

    EMB_SERVICE_TYPE_BUTT
} emb_service_type_t;

typedef struct {
    emb_service_type_t serviceType; /* type of service */
    char url[EMB_SERVICE_URL_MAX];  /* store url for proxy and server type, e.g. tcp://, rmda://, urma:// */
} emb_service_options_t;

#ifdef __cplusplus
}
#endif

#endif // MEMFABRIC_HYBRID_EMBRICKS_DEF_H
