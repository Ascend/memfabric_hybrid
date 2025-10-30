/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef DT_FUZZ_H
#define DT_FUZZ_H
#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI(#api, reinterpret_cast<TT>(api))

extern std::atomic<uint64_t> countMFFUzz;
// dt-fuzz 要求3小时或者3000万次
constexpr int CAPI_FUZZ_COUNT = 30000000;
constexpr int DT_RUNNING_TIME = 3 * 60 * 60;  // 3小时，单位秒
void MFFUzzCountNumber(char *name);
#endif  // DT_FUZZ_H
