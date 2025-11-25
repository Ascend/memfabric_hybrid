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
#ifndef ACC_LINKS_ACC_REF_H
#define ACC_LINKS_ACC_REF_H

#include <cstdint>
#include <utility>
#include <string.h>

namespace ock {
namespace acc {
/**
 * @brief Base class of referable object for smart ptr, all classes need to inherit from this class
 * if you want to use AccRef as smart ptr
 */
class AccReferable {
public:
    AccReferable() = default;
    virtual ~AccReferable() = default;

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

/**
 * @brief Smart ptr class
 */
template <typename T>
class AccRef {
public:
    /* constructor */
    AccRef() noexcept = default;

    /* dont fix: can't be explicit */
    AccRef(T* newObj) noexcept
    {
        // if new obj is not null, increase reference count and assign to mObj
        // else nothing need to do as mObj is nullptr by default
        if (newObj != nullptr) {
            newObj->IncreaseRef();
            mObj = newObj;
        }
    }

    AccRef(const AccRef<T> &other) noexcept
    {
        // if other's obj is not null, increase reference count and assign to mObj
        // else nothing need to do as mObj is nullptr by default
        if (other.mObj != nullptr) {
            other.mObj->IncreaseRef();
            mObj = other.mObj;
        }
    }

    AccRef(AccRef<T>&& other) noexcept : mObj(std::__exchange(other.mObj, nullptr))
    {
        // move constructor
        // since this mObj is null, just exchange
    }

    // de-constructor
    ~AccRef()
    {
        if (mObj != nullptr) {
            mObj->DecreaseRef();
        }
    }

    // operator =
    inline AccRef<T> &operator=(T* newObj)
    {
        this->Set(newObj);
        return *this;
    }

    inline AccRef<T> &operator=(const AccRef<T> &other)
    {
        if (this != &other) {
            this->Set(other.mObj);
        }
        return *this;
    }

    AccRef<T> &operator=(AccRef<T>&& other) noexcept
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
    inline bool operator==(const AccRef<T> &other) const
    {
        return mObj == other.mObj;
    }

    inline bool operator==(T* other) const
    {
        return mObj == other;
    }

    inline bool operator!=(const AccRef<T> &other) const
    {
        return mObj != other.mObj;
    }

    inline bool operator!=(T* other) const
    {
        return mObj != other;
    }

    // get operator and set
    inline T* operator->() const
    {
        return mObj;
    }

    inline T* Get() const
    {
        return mObj;
    }

    inline void Set(T* newObj)
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
    T* mObj = nullptr;
};

template <class Src, class Des>
static AccRef<Des> AccConvert(const AccRef<Src> &child)
{
    if (child.Get() != nullptr) {
        return AccRef<Des>(static_cast<Des*>(child.Get()));
    }
    return nullptr;
}
}  // namespace acc
}  // namespace ock
#endif  // ACC_LINKS_ACC_REF_H