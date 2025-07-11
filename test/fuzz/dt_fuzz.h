/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#ifndef DT_FUZZ_H
#define DT_FUZZ_H

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI(#api, reinterpret_cast<TT>(api))

// dt-fuzz 要求3小时或者3000万次
constexpr int CAPI_FUZZ_COUNT = 1;//0000000;
constexpr int DT_RUNNING_TIME = 3 * 60 * 60 * 60;  // 3小时，单位秒

#endif  // DT_FUZZ_H
