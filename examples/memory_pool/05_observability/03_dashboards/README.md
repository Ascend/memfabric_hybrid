# 03_dashboards

> **⚠️ 设计草案**：本文档描述的 5 类 dashboard JSON 文件尚未提交到仓库，当前仅作为设计方案参考。

## 场景
沉淀内存池统一仪表盘目录结构，便于不同环境快速复用与版本化管理。

## 目标
提供 dashboard 分层设计，区分总览、容量、性能、错误诊断与多租户视图。

## 交付物设计
- dashboards/overview.json：整体健康度与核心 KPI
- dashboards/performance.json：吞吐、P50/P95/P99、批大小分布
- dashboards/capacity.json：池容量水位、利用率、增长趋势
- dashboards/errors.json：错误码分布、失败率、异常时间窗
- dashboards/multi_rank.json：按 node/rank/device_id 下钻

## 使用能力
基于内存池相关指标与追踪数据进行可视化编排，不新增业务语义。

## 必要条件
Prometheus 与 tracing 数据源已接入 Grafana，且指标标签维度完整（cluster/node/rank/device_id）。

## 验收标准
- 5 类仪表盘均可加载且数据联动正常
- 支持从总览一键下钻到 rank 级别问题
- 支持按版本或环境做横向对比
