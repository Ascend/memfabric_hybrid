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
#ifndef MEMFABRIC_HYBRID_EMB_REF_H
#define MEMFABRIC_HYBRID_EMB_REF_H

#include <cstdint>

namespace ock {
namespace emb {

class EmReferable {
public:
    EmReferable() = default;
    virtual ~EmReferable() = default;

    inline void IncreaseRef()
    {
        __sync_fetch_and_add(&mRefCount, 1);
    }

    inline void DecreaseRef()
    {
        // delete itself if reference count equal to 0
        if (__sync_sub_and_fetch(&mRefCount, 1) == 0) {
            delete this;
        }
    }

protected:
    int32_t mRefCount = 0;
};

template<typename T>
class EmRef {
public:
    // constructor
    EmRef() noexcept = default;

    // fix: can't be explicit
    EmRef(T *newObj) noexcept
    {
        // if new obj is not null, increase reference count and assign to mObj
        // else nothing need to do as mObj is nullptr by default
        if (newObj != nullptr) {
            newObj->IncreaseRef();
            mObj = newObj;
        }
    }

    EmRef(const EmRef<T> &other) noexcept
    {
        // if other's obj is not null, increase reference count and assign to mObj
        // else nothing need to do as mObj is nullptr by default
        if (other.mObj != nullptr) {
            other.mObj->IncreaseRef();
            mObj = other.mObj;
        }
    }

    EmRef(EmRef<T> &&other) noexcept : mObj(std::__exchange(other.mObj, nullptr))
    {
        // move constructor
        // since this mObj is null, just exchange
    }

    // de-constructor
    ~EmRef()
    {
        if (mObj != nullptr) {
            mObj->DecreaseRef();
        }
    }

    // operator =
    inline EmRef<T> &operator=(T *newObj)
    {
        this->Set(newObj);
        return *this;
    }

    inline EmRef<T> &operator=(const EmRef<T> &other)
    {
        if (this != &other) {
            this->Set(other.mObj);
        }
        return *this;
    }

    EmRef<T> &operator=(EmRef<T> &&other) noexcept
    {
        if (this != &other) {
            auto tmp = mObj;
            mObj = std::__exchange(other.mObj, nullptr);
            if (tmp != nullptr) {
                tmp->DecreaseRef();
            }
        }
        return *this;
    }

    // equal operator
    inline bool operator==(const EmRef<T> &other) const
    {
        return mObj == other.mObj;
    }

    inline bool operator==(T *other) const
    {
        return mObj == other;
    }

    inline bool operator!=(const EmRef<T> &other) const
    {
        return mObj != other.mObj;
    }

    inline bool operator!=(T *other) const
    {
        return mObj != other;
    }

    // get operator and set
    inline T *operator->() const
    {
        return mObj;
    }

    inline T *Get() const
    {
        return mObj;
    }

    inline void Set(T *newObj)
    {
        if (newObj == mObj) {
            return;
        }

        if (newObj != nullptr) {
            newObj->IncreaseRef();
        }

        if (mObj != nullptr) {
            mObj->DecreaseRef();
        }

        mObj = newObj;
    }

private:
    T *mObj = nullptr;
};

template<class Src, class Des>
EmRef<Des> inline EmConvert(const EmRef<Src> &child)
{
    Des *converted = dynamic_cast<Des *>(child.Get());
    if (converted) {
        return EmRef<Des>(converted);
    }
    return nullptr;
}

template<typename C, typename... ARGS>
inline EmRef<C> EmMakeRef(ARGS... args)
{
    return new (std::nothrow) C(args...);
}

} // namespace emb
} // namespace ock
#endif // MEMFABRIC_HYBRID_EMB_REF_H
