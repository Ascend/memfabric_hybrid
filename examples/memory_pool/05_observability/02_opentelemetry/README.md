# 02_opentelemetry

## 场景
为内存池关键操作补充链路追踪与统一语义埋点，支持跨组件问题定位。

## 目标
提供 OpenTelemetry 接入设计，统一 trace/span/attributes 命名，并对接现有观测体系。

## 交付物设计
- otel_collector.yaml
  - 接收 OTLP（gRPC/HTTP）
  - 导出到 tracing 后端（如 Jaeger/Tempo）
- instrumentation_guideline.md
  - Span 命名规范（初始化、创建、join、batch copy、register/unregister）
  - 关键属性：rank、device_id、world_size、copy_type、batch_count、data_size

## 使用能力
围绕内存池接口调用路径做 tracing 设计，不新增业务语义。

## 必要条件
运行环境可部署 OpenTelemetry Collector，且应用进程可上报 OTLP 数据。

## 验收标准
- 能从一次请求追踪到关键内存池操作 span
- 错误 span 可携带错误码与上下文属性
- 可与 Prometheus/Grafana 指标形成关联分析
