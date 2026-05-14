#ifndef C10_CORE_ALLOCATOR_H_
#define C10_CORE_ALLOCATOR_H_

#include "c10/macros/Macros.h"

namespace c10 {

struct DataPtr {
    void *ptr_ = nullptr;
};

} // namespace c10

#endif // C10_CORE_ALLOCATOR_H_
