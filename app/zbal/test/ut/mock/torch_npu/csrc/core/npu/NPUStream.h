#ifndef TORCH_NPU_CSRC_CORE_NPU_NPU_STREAM_H_
#define TORCH_NPU_CSRC_CORE_NPU_NPU_STREAM_H_

#include <cstdint>
#include <functional>

namespace c10_npu {

class NPUStream {
public:
    NPUStream() = default;
    explicit NPUStream(int device) : device_(device) {}

    int device_index() const { return device_; }

    bool operator==(const NPUStream &other) const
    {
        return device_ == other.device_ && stream_ == other.stream_;
    }

    bool operator<(const NPUStream &other) const
    {
        return device_ < other.device_ || (device_ == other.device_ && stream_ < other.stream_);
    }

private:
    int device_ = 0;
    uintptr_t stream_ = 0;
};

} // namespace c10_npu

namespace std {
template <>
struct hash<c10_npu::NPUStream> {
    size_t operator()(const c10_npu::NPUStream &s) const noexcept
    {
        return std::hash<int>()(s.device_index());
    }
};
} // namespace std

#endif // TORCH_NPU_CSRC_CORE_NPU_NPU_STREAM_H_
