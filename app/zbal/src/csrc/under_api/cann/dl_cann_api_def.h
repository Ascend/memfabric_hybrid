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
#ifndef DL_CANN_API_DEF_H
#define DL_CANN_API_DEF_H

namespace zbal {

typedef enum {
    ACL_RT_DEV_RES_CUBE_CORE = 0, /* AI Core | Cube Core */
    ACL_RT_DEV_RES_VECTOR_CORE,   /* Vector Core */
} aclrtDevResType;

typedef enum {
    ACL_HOST_REGISTER_MAPPED = 0U, /* accessed by NPU */
} aclrtHostRegisterType;

} // namespace zbal

#endif // DL_CANN_API_DEF_H
