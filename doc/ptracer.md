# ptracer性能打点工具简介

## 功能概述
MemFabric内置的ptracer工具支持通过如下方式对代码进行打点，对关键函数调用的次数、成功失败情况、耗时等进行统计。
```c++
TP_TRACE_BEGIN(TP_HYBM_RDMA_LH_TO_GD);
ret = CopyLH2GD(params.src, params.dest, params.dataSize, options);
TP_TRACE_END(TP_HYBM_RDMA_LH_TO_GD, ret);
```
内置线程每隔固定周期将打点数据输出到ptracer独立日志中，为分析性能瓶颈、测试性能数据提供了一种简便的方法。

## 使用方法
ptracer是否开启受编译宏ENABLE_PTRACER控制，使用build_and_package_run.sh构建脚本时默认开启。

启用ptracer时，会在/var/log/memfabric_hybrid下生成ptracer_\<pid\>.dat形式的文件用于保存打点数据。

## 输出样例
以下是一次测试中ptracer输出的一个周期内的打点数据示例
```
TIME                   NAME                                    BEGIN          GOOD_END       BAD_END        ON_FLY         MIN(us)        MAX(us)        AVG(us)        TOTAL(us)
2026-01-05 14:57:03    TP_HYBM_RDMA_LH_TO_GD                   117            117            0              0              120.830        16193.220      713.658        83498.010
2026-01-05 14:57:03    TP_MMC_LOCAL_BATCH_PUT                  30             30             0              0              124.620        16400.930      3353.871       100616.120
2026-01-05 14:57:03    TP_ACC_SEND_ALLOC                       2              2              0              0              564.440        602.770        583.605        1167.210
```

## 字段描述

| keyword   | description   |
|-----------|---------------|
| TIME      | 时间            |
| NAME      | Tracepoint ID |
| BEGIN     | 调用次数          |
| GOOD_END  | 执行成功次数        |
| BAD_END   | 执行失败次数        |
| ON_FLY    | 执行中的数量        |
| MIN(us)   | 最低耗时（微秒）      |
| MAX(us)   | 最高耗时（微秒）      |
| AVG(us)   | 平均耗时（微秒）      |
| TOTAL(us) | 总计耗时（微秒）      |
