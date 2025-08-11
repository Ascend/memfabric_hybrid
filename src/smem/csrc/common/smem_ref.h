/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef MEMFABRIC_HYBRID_SMEM_REF_H
#define MEMFABRIC_HYBRID_SMEM_REF_H

#include <cstdint>
#include <utility>
#include <string.h>

namespace ock {
namespace smem {

class SmReferable {
public:
    SmReferable() = default;
    virtual ~SmReferable() = default;

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

template <typename T> class SmRef {
public:
    // constructor
    SmRef() noexcept = default;

    // fix: can't be explicit
    SmRef(T *newObj) noexcept
    {
        // if new obj is not null, increase reference count and assign to mObj
        // else nothing need to do as mObj is nullptr by default
        if (newObj != nullptr) {
            newObj->IncreaseRef();
            mObj = newObj;
        }
    }

    SmRef(const SmRef<T> &other) noexcept
    {
        // if other's obj is not null, increase reference count and assign to mObj
        // else nothing need to do as mObj is nullptr by default
        if (other.mObj != nullptr) {
            other.mObj->IncreaseRef();
            mObj = other.mObj;
        }
    }

    SmRef(SmRef<T> &&other) noexcept : mObj(std::__exchange(other.mObj, nullptr))
    {
        // move constructor
        // since this mObj is null, just exchange
    }

    // de-constructor
    ~SmRef()
    {
        if (mObj != nullptr) {
            mObj->DecreaseRef();
        }
    }

    // operator =
    inline SmRef<T> &operator = (T *newObj)
    {
        this->Set(newObj);
        return *this;
    }

    inline SmRef<T> &operator = (const SmRef<T> &other)
    {
        if (this != &other) {
            this->Set(other.mObj);
        }
        return *this;
    }

    SmRef<T> &operator = (SmRef<T> &&other) noexcept
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
    inline bool operator == (const SmRef<T> &other) const
    {
        return mObj == other.mObj;
    }

    inline bool operator == (T *other) const
    {
        return mObj == other;
    }

    inline bool operator != (const SmRef<T> &other) const
    {
        return mObj != other.mObj;
    }

    inline bool operator != (T *other) const
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

template <class Src, class Des> SmRef<Des> inline Convert(const SmRef<Src> &child)
{
    if (child.Get() != nullptr) {
        return SmRef<Des>(static_cast<Des *>(child.Get()));
    }
    return nullptr;
}

template <typename C, typename... ARGS> inline SmRef<C> SmMakeRef(ARGS... args)
{
    return new (std::nothrow) C(args...);
}

}
}
#endif // MEMFABRIC_HYBRID_SMEM_REF_H
