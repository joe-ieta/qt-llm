# 关键决策

## Host App 优先使用 RuntimeFacade

普通外部 Qt 应用不应直接拼 provider、HTTP executor 或 managed runtime。推荐路径是：

```text
Host App -> qtllm::host::RuntimeFacade -> QtLLMClient -> Provider / Runtime / HttpExecutor
```

这样可以让 qt-llm 统一负责 runtime 解释、模型发现、请求执行、流式解析和观测事件。

## ConversationClient 保持会话层职责

`ConversationClient` 面向聊天产品，负责会话、历史、profile、snapshot 和持久化。它不是所有 Host App 的必选层。

## 本地 llama.cpp 是内置托管 provider

`llama-cpp` provider 支持由 qt-llm 托管 `llama-server`。Host App 可以保存用户设置，但运行态解释和启动逻辑应属于 qt-llm。

## 工具调用独立成层

工具调用、MCP、ToolStudio、ToolsInside 不应污染简单聊天和普通模型调用路径。需要工具能力时再接入 `ToolEnabledChatEntry`。

## 文档中文重整

活跃文档改为中文维护。旧文档整体归档，避免新旧结论混杂。
