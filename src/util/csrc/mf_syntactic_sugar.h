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
#ifndef MEMFABRIC_HYBRID_MF_SYNTACTIC_SUGAR_H
#define MEMFABRIC_HYBRID_MF_SYNTACTIC_SUGAR_H
#include <utility>

namespace ock {
namespace mf {
template<typename F>
struct Defer {
    F f;
    Defer(F &&f) : f(std::forward<F>(f)) {}
    ~Defer()
    {
        f();
    }
};

template<typename F>
Defer<F> defer(F &&f)
{
    return Defer<F>(std::forward<F>(f));
}
} // namespace mf
} // namespace ock
#endif //MEMFABRIC_HYBRID_MF_SYNTACTIC_SUGAR_H
