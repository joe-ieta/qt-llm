# qt-llm 文档

本目录保存 qt-llm 当前有效的中文文档。`docs/archive/` 只用于历史追溯，不作为当前能力、接口或验证结论的依据。

## 外部开发者

1. [项目定位](./00-project/positioning.md)
2. [对外开发接口与集成](./20-integration/public-api-guide.md)
3. [Host App 集成](./20-integration/host-app-guide.md)
4. [CMake 安装包与源码集成](./30-development/cmake-consumption.md)
5. [配置参考](./50-reference/config-reference.md)
6. [API 索引](./50-reference/api-index.md)

## 按能力阅读

- 请求级异步集成：[请求句柄](./30-development/request-handle.md)
- 底层请求生命周期：[请求生命周期](./30-development/request-lifecycle.md)
- 多会话与历史：[ConversationClient](./20-integration/conversationclient-guide.md)
- 本地 llama.cpp：[托管本地运行时](./20-integration/local-llamacpp-guide.md)
- 工具与 MCP：[工具和 MCP 集成](./20-integration/tool-mcp-guide.md)
- 结构化结果：[结构化输出](./30-development/structured-output.md)
- 上下文预算：[上下文窗口](./30-development/context-window-management.md)
- 诊断事件：[事件与诊断](./30-development/event-diagnostics.md)

## 工程开发者

1. [顶层约束](../AI_RULES.md)
2. [总体架构](./10-architecture/overview.md)
3. [构建和测试](./30-development/build-and-test.md)
4. [编码规范](./30-development/coding-guidelines.md)
5. [发布检查清单](./30-development/release-checklist.md)
6. [自动发布验证](./30-development/release-validation.md)

## 优化方案状态

- [通用 LLM 基础能力优化方案](./00-project/llm-foundation-optimization-plan.md)：qt-llm 侧 QTL-00 至 QTL-10 已全部实施完成。
- [qt-llm 工作包](./00-project/qtllm-optimization-work-packages.md)：实现内容、依赖和验收结果。
- [工作包完成状态](./00-project/qtllm-work-package-status.md)：当前为 11/11（100%）。
- [CoReader 工作包](./00-project/coreader-integration-work-packages.md)：独立的上层迁移计划，不计入 qt-llm 完成状态。

## 文档分区

- [项目与决策](./00-project/)
- [架构](./10-architecture/)
- [外部集成](./20-integration/)
- [工程开发](./30-development/)
- [示例与工具应用](./40-apps/)
- [参考资料](./50-reference/)
- [发布说明](./releases/)
- [历史归档](./archive/)
