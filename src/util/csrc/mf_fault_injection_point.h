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

#ifndef MEMFABRIC_HYBRID_SRC_UTIL_CSRC_MF_FAULT_INJECTION_POINT_H_
#define MEMFABRIC_HYBRID_SRC_UTIL_CSRC_MF_FAULT_INJECTION_POINT_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <tuple>
#include <type_traits>
#include <typeindex>
#include <utility>

namespace ock {
namespace mf {

constexpr std::size_t FAULT_INJECTION_POINT_PARAM_SIZE = 32UL;

enum class FaultInjectionPointStatus : int32_t {
    OK = 0,
    ERROR = -1,
    CALLBACK_NULL = 2,
    NOT_FOUND = 3,
};

enum class FaultInjectionPointType : int32_t {
    CALLBACK = 0,
    RESET,
    PAUSE,
    ABORT,
    BUTT,
};

struct FaultInjectionPointParam {
    char paramData[FAULT_INJECTION_POINT_PARAM_SIZE]{};
};

struct FaultInjectionPointExecution {
    bool skipBlock = false;
};

namespace detail {

template<typename... Args>
struct CallbackSignatureTag {};

class FaultInjectionPointCallbackBase {
public:
    virtual ~FaultInjectionPointCallbackBase() = default;
    virtual std::type_index GetSignature() const = 0;
    virtual uintptr_t GetKey() const = 0;
    virtual void Invoke(FaultInjectionPointParam *userParam, void **args) const = 0;
};

template<typename... Args>
class FaultInjectionPointCallback final : public FaultInjectionPointCallbackBase {
public:
    using CallbackType = void (*)(FaultInjectionPointParam *, Args...);

    explicit FaultInjectionPointCallback(CallbackType callback) : callback_(callback) {}

    std::type_index GetSignature() const override
    {
        return std::type_index(typeid(CallbackSignatureTag<typename std::decay<Args>::type...>));
    }

    uintptr_t GetKey() const override
    {
        return reinterpret_cast<uintptr_t>(callback_);
    }

    void Invoke(FaultInjectionPointParam *userParam, void **args) const override
    {
        InvokeImpl(userParam, args, std::index_sequence_for<Args...>{});
    }

private:
    template<std::size_t... Indexes>
    void InvokeImpl(FaultInjectionPointParam *userParam, void **args, std::index_sequence<Indexes...>) const
    {
        callback_(userParam, (*reinterpret_cast<typename std::decay<Args>::type *>(args[Indexes]))...);
    }

    CallbackType callback_;
};

template<typename Tuple, std::size_t... Indexes>
inline void FillArgPointers(void **args, Tuple &tuple, std::index_sequence<Indexes...>)
{
    int unused[] = {0, ((args[Indexes] = static_cast<void *>(&std::get<Indexes>(tuple))), 0)...};
    (void)unused;
}

template<typename... Args>
inline std::type_index GetSignatureType()
{
    return std::type_index(typeid(CallbackSignatureTag<typename std::decay<Args>::type...>));
}

} // namespace detail

class FaultInjectionPointManager {
public:
    static FaultInjectionPointStatus Init();
    static FaultInjectionPointStatus Exit();
    static FaultInjectionPointStatus Reload();

    static FaultInjectionPointStatus Register(const std::string &name, const std::string &desc);

    template<typename... Args>
    static FaultInjectionPointStatus Register(const std::string &name, const std::string &desc,
                                              void (*callback)(FaultInjectionPointParam *, Args...))
    {
#ifdef MF_ENABLE_TRACEPOINT
        if (callback == nullptr) {
            return FaultInjectionPointStatus::CALLBACK_NULL;
        }
        return RegisterImpl(name, desc, std::make_shared<detail::FaultInjectionPointCallback<Args...>>(callback));
#else
        (void)name;
        (void)desc;
        (void)callback;
        return FaultInjectionPointStatus::OK;
#endif
    }

    static FaultInjectionPointStatus Unregister(const std::string &name);
    static FaultInjectionPointStatus Activate(const std::string &name, FaultInjectionPointType type, uint32_t timeAlive,
                                              const FaultInjectionPointParam &userParam = {});
    static FaultInjectionPointStatus Deactivate(const std::string &name);
    static FaultInjectionPointStatus DeactivateAll();
    static bool IsActive(const std::string &name);
    static FaultInjectionPointParam MakeParam(const std::string &value);

    template<typename... Args>
    static FaultInjectionPointExecution Begin(const char *name, Args &&...args)
    {
#ifdef MF_ENABLE_TRACEPOINT
        using TupleType = std::tuple<typename std::decay<Args>::type...>;
        TupleType storedArgs(std::forward<Args>(args)...);
        void *argPointers[sizeof...(Args) == 0 ? 1 : sizeof...(Args)] = {};
        detail::FillArgPointers(argPointers, storedArgs, std::index_sequence_for<Args...>{});
        return BeginImpl(name, detail::GetSignatureType<Args...>(), argPointers);
#else
        (void)name;
        int unused[] = {0, ((void)args, 0)...};
        (void)unused;
        return FaultInjectionPointExecution{};
#endif
    }

private:
    static FaultInjectionPointStatus
    RegisterImpl(const std::string &name, const std::string &desc,
                 const std::shared_ptr<detail::FaultInjectionPointCallbackBase> &callback);
    static FaultInjectionPointExecution BeginImpl(const char *name, const std::type_index &signature, void **args);
};

} // namespace mf
} // namespace ock

#ifdef MF_ENABLE_TRACEPOINT
#define FIP_START(name, ...)                                                     \
    do {                                                                         \
        ::ock::mf::FaultInjectionPointExecution faultInjectionPointExecution__ = \
            ::ock::mf::FaultInjectionPointManager::Begin(#name, ##__VA_ARGS__);  \
        if (!faultInjectionPointExecution__.skipBlock) {
#define FIP_END \
    }           \
    }           \
    while (0)
#else
#define FIP_START(name, ...) \
    do {                     \
        if (true) {
#define FIP_END \
    }           \
    }           \
    while (0)
#endif

#endif // MEMFABRIC_HYBRID_SRC_UTIL_CSRC_MF_FAULT_INJECTION_POINT_H_
