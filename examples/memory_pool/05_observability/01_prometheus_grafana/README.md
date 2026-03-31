# 01_prometheus_grafana

## 场景
为内存池样例提供最小可落地可观测方案，统一指标采集与看板展示。

## 目标
交付监控资产模板：prometheus.yaml 与 grafana_dashboard.json，用于后续所有内存池样例复用。

## 交付物设计
- prometheus.yaml
  - 内存池进程指标抓取任务
  - 作业标签：cluster、node、rank、device_id
- grafana_dashboard.json
  - 面板1：吞吐（req/s, bytes/s）
  - 面板2：时延（P50/P95/P99）
  - 面板3：错误率与错误码分布
  - 面板4：池容量水位（已用/总量）

## 使用能力（仅内存池）
基于内存池示例的运行数据做观测，不新增业务接口。

## 必要条件
测试环境可部署 Prometheus + Grafana，且内存池运行进程对指标端点可访问。

## 验收标准
- 启动后 5 分钟内看板有稳定数据
- 可按 rank/node 下钻定位问题
- 关键指标可支持回归对比（版本A/B）
