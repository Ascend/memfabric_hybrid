#ifndef TORCH_NPU_CSRC_CORE_NPU_SYS_CTRL_NPU_SYS_CTRL_H_
#define TORCH_NPU_CSRC_CORE_NPU_SYS_CTRL_NPU_SYS_CTRL_H_

namespace c10_npu {
namespace NpuSysCtrl {

class SysCtrl {
public:
    static SysCtrl &GetInstance()
    {
        static SysCtrl instance;
        return instance;
    }
    bool GetInitFlag() { return true; }
};

} // namespace NpuSysCtrl
} // namespace c10_npu

#endif // TORCH_NPU_CSRC_CORE_NPU_SYS_CTRL_NPU_SYS_CTRL_H_
