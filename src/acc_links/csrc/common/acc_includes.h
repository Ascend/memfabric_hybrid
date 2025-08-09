/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef ACC_LINKS_ACC_INCLUDES_H
#define ACC_LINKS_ACC_INCLUDES_H

#include <arpa/inet.h>
#include <atomic>
#include <cstdint>
#include <netinet/in.h>
#include <set>
#include <string>
#include <thread>

#include "acc_def.h"
#include "acc_tcp_client.h"
#include "acc_tcp_link.h"
#include "acc_tcp_request_context.h"
#include "acc_tcp_server.h"
#include "acc_tcp_shared_buf.h"

#include "acc_out_logger.h"

namespace ock {
namespace acc {
using Result = int32_t;

/**
 * @brief New an object return with ref object
 *
 * @param args             [in] args of object
 * @return Ref object, if new failed internal, an empty Ref object will be returned
 */
template <typename C, typename... ARGS>
inline AccRef<C> AccMakeRef(ARGS... args)
{
    return new (std::nothrow) C(args...);
}

#ifndef LIKELY
#define LIKELY(x) (__builtin_expect(!!(x), 1) != 0)
#endif

#ifndef UNLIKELY
#define UNLIKELY(x) (__builtin_expect(!!(x), 0) != 0)
#endif
}  // namespace acc
}  // namespace ock

#endif  // ACC_LINKS_ACC_INCLUDES_H
