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
        __atomic_add_fetch(&refCount_, 1, __ATOMIC_RELAXED);
    }

    inline void DecreaseRef()
    {
        if (__atomic_sub_fetch(&refCount_, 1, __ATOMIC_ACQ_REL) == 0) {
            delete this;
        }
    }

protected:
    int32_t refCount_ = 0;
};

template<typename T>
class EmRef {
public:
    // constructor
    EmRef() noexcept = default;

    // fix: can't be explicit
    EmRef(T *newObj) noexcept
    {
        // if new obj is not null, increase reference count and assign to obj_
        // else nothing need to do as obj_ is nullptr by default
        if (newObj != nullptr) {
            newObj->IncreaseRef();
            obj_ = newObj;
        }
    }

    EmRef(const EmRef<T> &other) noexcept
    {
        // if other's obj is not null, increase reference count and assign to obj_
        // else nothing need to do as obj_ is nullptr by default
        if (other.obj_ != nullptr) {
            other.obj_->IncreaseRef();
            obj_ = other.obj_;
        }
    }

    EmRef(EmRef<T> &&other) noexcept : obj_(std::__exchange(other.obj_, nullptr))
    {
        // move constructor
        // since this obj_ is null, just exchange
    }

    // de-constructor
    ~EmRef()
    {
        if (obj_ != nullptr) {
            obj_->DecreaseRef();
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
            this->Set(other.obj_);
        }
        return *this;
    }

    EmRef<T> &operator=(EmRef<T> &&other) noexcept
    {
        if (this != &other) {
            auto tmp = obj_;
            obj_ = std::__exchange(other.obj_, nullptr);
            if (tmp != nullptr) {
                tmp->DecreaseRef();
            }
        }
        return *this;
    }

    // equal operator
    inline bool operator==(const EmRef<T> &other) const
    {
        return obj_ == other.obj_;
    }

    inline bool operator==(T *other) const
    {
        return obj_ == other;
    }

    inline bool operator!=(const EmRef<T> &other) const
    {
        return obj_ != other.obj_;
    }

    inline bool operator!=(T *other) const
    {
        return obj_ != other;
    }

    // get operator and set
    inline T *operator->() const
    {
        return obj_;
    }

    inline T *Get() const
    {
        return obj_;
    }

    inline void Set(T *newObj)
    {
        if (newObj == obj_) {
            return;
        }

        if (newObj != nullptr) {
            newObj->IncreaseRef();
        }

        if (obj_ != nullptr) {
            obj_->DecreaseRef();
        }

        obj_ = newObj;
    }

private:
    T *obj_ = nullptr;
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
