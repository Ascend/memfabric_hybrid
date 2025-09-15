/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef __SMEM_HYBM_CORE_API_H__
#define __SMEM_HYBM_CORE_API_H__

#include "smem_bm_def.h"
#include "mmc_common_includes.h"

namespace ock {
namespace mmc {

using smemInitFunc = int32_t (*)(uint32_t);
using smemUnInitFunc = void (*)();
using smemSetExternLoggerFunc = int32_t (*)(void (*)(int level, const char *));
using smemSetLogLevelFunc = int32_t (*)(int);
using smemGetLastErrMsgFunc = const char *(*)();
using smemGetAndClearLastErrMsgFunc = const char *(*)();

using smemBmConfigInitFunc = int32_t (*)(smem_bm_config_t *);
using smemBmInitFunc = int32_t (*)(const char *, uint32_t, uint16_t, const smem_bm_config_t *);
using smemBmUnInitFunc = void (*)(uint32_t);
using smemBmGetRankIdFunc = uint32_t (*)();
using smemBmCreateFunc = smem_bm_t (*)(uint32_t, uint32_t, smem_bm_data_op_type, uint64_t, uint64_t, uint32_t);
using smemBmDestroyFunc = void (*)(smem_bm_t);
using smemBmJoinFunc = int32_t (*)(smem_bm_t, uint32_t, void **);
using smemBmLeaveFunc = int32_t (*)(smem_bm_t, uint32_t);
using smemBmGetLocalMemSizeFunc = uint64_t (*)(smem_bm_t);
using smemBmPtrFunc = void *(*)(smem_bm_t, uint16_t);
using smemBmCopyFunc = int32_t (*)(smem_bm_t, const void *, void *, uint64_t, smem_bm_copy_type, uint32_t);
using smemBmCopy2dFunc = int32_t (*)(smem_bm_t, const void *, uint64_t, void *, uint64_t,
                                     uint64_t, uint64_t, smem_bm_copy_type, uint32_t);

class MFSmemApi {
public:
    static Result LoadLibrary(const std::string &libDirPath);

    /**
     * @brief Initialize the smem running environment
     *
     * @param flags            [in] optional flags, reserved
     * @return 0 if successful,
     */
    static int32_t SmemInit(uint32_t flags)
    {
        return gSmemInit(flags);
    }

    /**
     * @brief Set external log function, user can set customized logger function,
     * in the customized logger function, user can use unified logger utility,
     * then the log message can be written into the same log file as caller's,
     * if it is not set, acc_links log message will be printed to stdout.
     *
     * level description:
     * 0 DEBUG,
     * 1 INFO,
     * 2 WARN,
     * 3 ERROR
     *
     * @param func             [in] external logger function
     * @return 0 if successful
     */
    static int32_t SmemSetExternLogger(void (*func)(int level, const char *msg))
    {
        return gSmemSetExternLogger(func);
    }

    /**
     * @brief set log print level
     *
     * @param level            [in] log level, 0:debug 1:info 2:warn 3:error
     * @return 0 if successful
     */
    static int32_t SmemSetLogLevel(int level)
    {
        return gSmemSetLogLevel(level);
    }

    /**
     * @brief Un-Initialize the smem running environment
     */
    static void SmemUninit()
    {
        gSmemUnInit();
    }

    /**
     * @brief Get the last error message
     *
     * @return last error message
     */
    static const char *SmemGetLastErrMsg()
    {
        return gSemGetLastErrMsg();
    }

    /**
     * @brief Get the last error message and clear
     *
     * @return last error message
     */
    static const char *smem_get_and_clear_last_err_msg()
    {
        return gSmemGetAndClearLastErrMsg();
    }

    /**
     * @brief Set the config to default values
     *
     * @param config           [in] the config to be set
     * @return 0 if successful
     */
    static int32_t SmemBmConfigInit(smem_bm_config_t *config)
    {
        return gSmemBmConfigInit(config);
    }

    /**
     * @brief Initialize Big Memory library, which composes device memory on many NPUs and host memory on many hosts
     * into global shared memory space for high throughput data store or data transfer.
     * For instance, user can store KVCache into the global shared memory for reuse. i.e. Once a worker generates
     * the KVBlocks then copy to global shared memory space, other workers can read it
     * by data copy as well.
     *
     * @param storeURL         [in] configure store url for control, e.g. tcp:://ip:port
     * @param worldSize        [in] number of guys participating
     * @param deviceId         [in] device id
     * @param config           [in] extract config
     * @return 0 if successful
     */
    static int32_t SmemBmInit(const char *storeURL, uint32_t worldSize, uint16_t deviceId,
                              const smem_bm_config_t *config)
    {
        return gSmemBmInit(storeURL, worldSize, deviceId, config);
    }

    /**
     * @brief Un-initialize the Big Memory library
     *
     * @param flags            [in] optional flags, not used yet
     */
    static void SmemBmUninit(uint32_t flags);

    /**
     * @brief Get the rank id, assigned during initialization i.e. after call <i>smem_bm_init</i>
     *
     * @return rank id if successful, UINT32_MAX is returned if failed
     */
    static uint32_t SmemBmGetRankId(void)
    {
        return gSmemBmGetRankId();
    }

    /**
     * @brief Create a Big Memory object locally after initialized, this only create local memory segment and after
     * call <i>smem_bm_join</i> the local memory segment will be joined into global space. One Big Memory object is
     * a global memory space, data operation does work across different Big Memory object.
     * We need to specify different <i>id</i> for different Big Memory object.
     *
     * @param id               [in] identity of the Big Memory object
     * @param memberSize       [in] number of guys participating, which should equal or less the world size
     * @param memType          [in] memory type, device HBM memory, host dram memory or both
     * @param dataOpType       [in] data operation type, SDMA or RoCE etc
     * @param localMemorySize  [in] the size of local memory contributes to Big Memory object
     * @param flags            [in] optional flags
     * @return Big Memory object handle if successful, nullptr if failed
     */
    static smem_bm_t SmemBmCreate(uint32_t id, uint32_t memberSize, smem_bm_data_op_type dataOpType,
                                  uint64_t localDRAMSize, uint64_t localHBMSize, uint32_t flags)
    {
        return gSmemBmCreate(id, memberSize, dataOpType, localDRAMSize, localHBMSize, flags);
    }

    /**
     * @brief Destroy the Big Memory object
     *
     * @param handle           [in] the Big Memory object to be destroyed
     */
    static void SmemBmDestroy(smem_bm_t handle)
    {
        return gSmemBmDestroy(handle);
    }

    /**
     * @brief Join to global Big Memory space actively, after this, we can operate on the global space,
     * i.e. use <i>smem_bm_ptr</i> to get peer gva address and use <i>smem_bm_copy</i> to do data copy
     *
     * @param handle           [in] Big Memory object handle created by <i>smem_bm_create</i>
     * @param flags            [in] optional flags
     * @param localGvaAddress  [out] local part of the global virtual memory space
     * @return 0 if successful
     */
    static int32_t SmemBmJoin(smem_bm_t handle, uint32_t flags, void **localGvaAddress)
    {
        return gSmemBmJoin(handle, flags, localGvaAddress);
    }

    /**
     * @brief Leave the global Big Memory space actively, after this, we cannot operate on the global space anymore
     *
     * @param handle           [in] Big Memory object handle created by <i>SmemBmcreate</i>
     * @param flags            [in] optional flags
     * @return 0 if successful
     */
    static int32_t SmemBmLeave(smem_bm_t handle, uint32_t flags)
    {
        return gSmemBmLeave(handle, flags);
    }

    /**
     * @brief Get size of local memory segment that contributed to global space
     *
     * @param handle           [in] Big Memory object handle created by <i>SmemBmcreate</i>
     * @return local memory size in bytes
     */
    static uint64_t SmemBmGetLocalMemSize(smem_bm_t handle)
    {
        return gSmemBmGetLocalMemSizeFunc(handle);
    }

    /**
     * @brief Get peer gva of peer memory segment by rank id
     *
     * @param handle           [in] Big Memory object handle created by <i>smem_bm_create</i>
     * @param peerRankId       [in] rank id of peer
     * @return ptr of peer gva
     */
    static void *SmemBmPtr(smem_bm_t handle, uint16_t peerRankId)
    {
        return gSmemBmPtr(handle, peerRankId);
    }

    /**
     * @brief Data copy on Big Memory object, several copy types supported:
     * L2G: local memory to global space
     * G2L: global space to local memory
     * G2H: global space to host memory
     * H2G: host memory to global space
     *
     * @param handle           [in] Big Memory object handle created by <i>smem_bm_create</i>
     * @param src              [in] source gva of data
     * @param dest             [in] target gva of data
     * @param size             [in] size of data to be copied
     * @param t                [in] copy type, L2G, G2L, G2H, H2G
     * @param flags            [in] optional flags
     * @return 0 if successful
     */
    static int32_t SmemBmCopy(smem_bm_t handle, const void *src, void *dest, uint64_t size, smem_bm_copy_type t,
                              uint32_t flags)
    {
        return gSmemBmCopy(handle, src, dest, size, t, flags);
    }

    /**
     * @brief Data copy on Big Memory object, several copy types supported:
     * L2G: local memory to global space
     * G2L: global space to local memory
     * G2H: global space to host memory
     * H2G: host memory to global space
     *
     * @param handle           [in] Big Memory object handle created by <i>smem_bm_create</i>
     * @param src              [in] source gva of data
     * @param spitch           [in] pitch of source memory
     * @param dest             [in] target gva of data
     * @param dpitch           [in] pitch of destination memory
     * @param width            [in] width of matrix transfer
     * @param heigth           [in] height of matrix transfer
     * @param t                [in] copy type, L2G, G2L, G2H, H2G
     * @param flags            [in] optional flags
     * @return 0 if successful
     */
    int32_t smem_bm_copy_2d(smem_bm_t handle, const void *src, uint64_t spitch,
                            void *dest, uint64_t dpitch, uint64_t width, uint64_t heigth,
                            smem_bm_copy_type t, uint32_t flags)
    {
        return gSmemBmCopy2d(handle, src, spitch, dest, dpitch, width, heigth, t, flags);
    }

private:
    static int32_t GetLibPath(const std::string &libDir, std::string &outputPath);

private:
    static std::mutex gMutex;
    static bool gLoaded;
    static void *gSmemHandle;
    static const char *gSmemLibName;

    static smemInitFunc gSmemInit;
    static smemUnInitFunc gSmemUnInit;
    static smemSetExternLoggerFunc gSmemSetExternLogger;
    static smemSetLogLevelFunc gSmemSetLogLevel;
    static smemGetLastErrMsgFunc gSemGetLastErrMsg;
    static smemGetAndClearLastErrMsgFunc gSmemGetAndClearLastErrMsg;

    static smemBmConfigInitFunc gSmemBmConfigInit;
    static smemBmInitFunc gSmemBmInit;
    static smemBmUnInitFunc gSmemBmUnInit;
    static smemBmGetRankIdFunc gSmemBmGetRankId;
    static smemBmCreateFunc gSmemBmCreate;
    static smemBmDestroyFunc gSmemBmDestroy;
    static smemBmJoinFunc gSmemBmJoin;
    static smemBmLeaveFunc gSmemBmLeave;
    static smemBmGetLocalMemSizeFunc gSmemBmGetLocalMemSizeFunc;
    static smemBmPtrFunc gSmemBmPtr;
    static smemBmCopyFunc gSmemBmCopy;
    static smemBmCopy2dFunc gSmemBmCopy2d;
};
}
}

#endif  // __SMEM_HYBM_CORE_API_H__
