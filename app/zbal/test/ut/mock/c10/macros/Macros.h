#ifndef C10_MACROS_MACROS_H_
#define C10_MACROS_MACROS_H_

#include <cstdint>
#include <memory>

namespace c10 {

class GatheredContext {
public:
    GatheredContext() = default;
    virtual ~GatheredContext() = default;
};

} // namespace c10

#endif // C10_MACROS_MACROS_H_
