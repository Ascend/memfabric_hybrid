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

#ifndef MEMFABRIC_HYBRID_SCOPE_GUARD_H
#define MEMFABRIC_HYBRID_SCOPE_GUARD_H

#include <atomic>
#include <thread>
#include <functional>

namespace ock {
namespace mf {

template<typename T>
class ScopeGuard {
public:
    explicit ScopeGuard(T resource, std::function<void(T)> deleter)
        : deleter_(std::move(deleter)), resource_(std::move(resource)), owned_(true)
    {
        static_assert(std::is_nothrow_move_constructible_v<T>,
                      "T must be nothrow move constructible for exception safety");
    }

    ScopeGuard(const ScopeGuard &) = delete;
    ScopeGuard &operator=(const ScopeGuard &) = delete;

    ScopeGuard(ScopeGuard &&other) noexcept
        : resource_(std::move(other.resource_)), deleter_(std::move(other.deleter_)), owned_(other.owned_)
    {
        other.owned_ = false;
    }
    ScopeGuard &operator=(ScopeGuard &&other) noexcept
    {
        if (this != &other) {
            if (owned_) {
                execute_deleter();
            }
            resource_ = std::move(other.resource_);
            deleter_ = std::move(other.deleter_);
            owned_ = other.owned_;
            other.owned_ = false;
        }
        return *this;
    }

    ~ScopeGuard() noexcept
    {
        if (owned_) {
            execute_deleter();
        }
    }

    void release() noexcept
    {
        owned_ = false;
    }

    void execute() noexcept
    {
        if (owned_) {
            execute_deleter();
            owned_ = false;
        }
    }

    T abandon() noexcept
    {
        owned_ = false;
        return resource_;
    }

    T get() const noexcept
    {
        return resource_;
    }

    explicit operator bool() const noexcept
    {
        return owned_;
    }

private:
    void execute_deleter() noexcept
    {
        if (deleter_) {
            static_cast<void>(deleter_(resource_));
        }
    }

    std::function<void(T)> deleter_;
    T resource_;
    bool owned_{false};
};

template<typename T, typename D>
inline ScopeGuard<T> make_scope_guard(T resource, D deleter)
{
    return ScopeGuard<T>(std::move(resource), std::function<void(T)>(std::move(deleter)));
}

} // namespace mf
} // namespace ock

#endif // MEMFABRIC_HYBRID_SCOPE_GUARD_H