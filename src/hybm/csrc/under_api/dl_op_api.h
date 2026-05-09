/*
* Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
* MemFabric_Hybrid is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*          http://license.coscl.org.cn/MulanPSL2
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
* See the Mulan PSL v2 for more details.
*/

#ifndef MF_HYBM_CORE_DL_OP_API_H
#define MF_HYBM_CORE_DL_OP_API_H

#include <mutex>
#include "hybm_common_include.h"

namespace ock {
namespace mf {

struct aclTensor;
struct aclOpExecutor;

typedef enum {
    ACL_DT_UNDEFINED = -1,
    ACL_FLOAT = 0,
    ACL_FLOAT16 = 1,
    ACL_INT8 = 2,
    ACL_INT32 = 3,
    ACL_UINT8 = 4,
    ACL_INT16 = 6,
    ACL_UINT16 = 7,
    ACL_UINT32 = 8,
    ACL_INT64 = 9,
    ACL_UINT64 = 10,
    ACL_DOUBLE = 11,
    ACL_BOOL = 12,
    ACL_STRING = 13,
    ACL_COMPLEX64 = 16,
    ACL_COMPLEX128 = 17,
    ACL_BF16 = 27,
    ACL_INT4 = 29,
    ACL_UINT1 = 30,
    ACL_COMPLEX32 = 33,
} aclDataType;

typedef enum {
    ACL_FORMAT_UNDEFINED = -1,
    ACL_FORMAT_NCHW = 0,
    ACL_FORMAT_NHWC = 1,
    ACL_FORMAT_ND = 2,
    ACL_FORMAT_NC1HWC0 = 3,
} aclFormat;

using aclnnShmemSdmaStarsQueryGetWorkspaceSizeFunc = Result (*)(const aclTensor *, aclTensor *, uint64_t *,
                                                                aclOpExecutor **);
using aclnnShmemSdmaStarsQueryFunc = Result (*)(void *, uint64_t, aclOpExecutor *, void *);

using aclCreateTensorFunc = aclTensor *(*)(const int64_t *, uint64_t, aclDataType, const int64_t *, int64_t, aclFormat,
                                           const int64_t *, uint64_t, void *);

using aclDestroyTensorFunc = Result (*)(aclTensor *);

class DlOpApi {
public:
    static Result LoadLibrary();
    static void CleanupLibrary();

    static inline Result AclnnShmemSdmaStarsQueryGetWorkspaceSize(const aclTensor *input, aclTensor *out,
                                                                  uint64_t *workspaceSize, aclOpExecutor **executor)
    {
        if (pAclnnShmemSdmaStarsQueryGetWorkspaceSize == nullptr) {
            return BM_DL_FUNCTION_FAILED;
        }
        return pAclnnShmemSdmaStarsQueryGetWorkspaceSize(input, out, workspaceSize, executor);
    }

    static inline Result AclnnShmemSdmaStarsQuery(void *workspace, uint64_t workspaceSize, aclOpExecutor *executor,
                                                  void *stream)
    {
        if (pAclnnShmemSdmaStarsQuery == nullptr) {
            return BM_DL_FUNCTION_FAILED;
        }
        return pAclnnShmemSdmaStarsQuery(workspace, workspaceSize, executor, stream);
    }

    static inline aclTensor *AclCreateTensor(const int64_t *viewDims, uint64_t viewDimsNum, aclDataType dataType,
                                             const int64_t *stride, int64_t offset, aclFormat format,
                                             const int64_t *storageDims, uint64_t storageDimsNum, void *tensorData)
    {
        if (pAclCreateTensor == nullptr) {
            return nullptr;
        }
        return pAclCreateTensor(viewDims, viewDimsNum, dataType, stride, offset, format, storageDims, storageDimsNum,
                                tensorData);
    }

    static inline Result AclDestroyTensor(aclTensor *tensorData)
    {
        if (pAclDestroyTensor == nullptr) {
            return BM_DL_FUNCTION_FAILED;
        }
        return pAclDestroyTensor(tensorData);
    }

private:
    static std::mutex gMutex;
    static bool gLoaded;
    static void *opapiHandle;
    static void *opbaseHandle;
    static const char *gOpapiLibName;
    static const char *gOpBaseLibName;

    static aclnnShmemSdmaStarsQueryGetWorkspaceSizeFunc pAclnnShmemSdmaStarsQueryGetWorkspaceSize;
    static aclnnShmemSdmaStarsQueryFunc pAclnnShmemSdmaStarsQuery;
    static aclCreateTensorFunc pAclCreateTensor;
    static aclDestroyTensorFunc pAclDestroyTensor;
};
} // namespace mf
} // namespace ock

#endif // MF_HYBM_CORE_DL_OP_API_H