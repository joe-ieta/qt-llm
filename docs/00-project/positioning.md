# 项目定位

qt-llm 是一个面向 Qt/C++ 桌面应用和内部工具的 LLM 集成基础库。

它解决的问题是：Host App 不应重复实现 provider 选择、请求 payload 拼装、HTTP 执行、流式解析、本地模型服务启动、模型发现、会话管理、工具调用和运行观测。qt-llm 应把这些能力封装为 Qt 原生接口，让应用只组合自己需要的层。

## 目标

- 将与 LLM 交互相关的通用工作封装在 `src/qtllm/`。
- 提供稳定简单的对外 API：`RuntimeFacade`、`LlmConfig`、`QtLLMClient`、`ConversationClient`。
- 强化本地托管 provider，当前重点是内置 `llama.cpp`。
- 让执行过程可感知、可观测、可控制。
- 保持 UI、Agent、工具系统和底层 runtime 的边界清晰。

## 非目标

- 不做 Web 优先的中心化 LLM 网关。
- 不把业务 Agent 逻辑混入核心库。
- 不要求 Host App 理解所有 provider 内部差异。
- 不把示例 App 当成唯一使用方式。
- 不用单一 provider 绑死项目能力。
