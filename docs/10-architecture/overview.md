# 总体架构

qt-llm 采用分层结构：

```text
Host App / 示例 App / Agent
  -> Host Facade / Conversation / Tool Entry
  -> QtLLMClient
  -> Provider
  -> HttpExecutor / Managed Runtime
```

## 核心目录

- `src/qtllm/core/`：`LlmConfig`、请求类型、`QtLLMClient`。
- `src/qtllm/host/`：`RuntimeFacade`、`RuntimeProfile`、模型目录服务。
- `src/qtllm/providers/`：各 provider 实现和 provider factory。
- `src/qtllm/runtime/`：本地托管 runtime，当前重点是 llama.cpp。
- `src/qtllm/chat/`：会话、历史、快照和 client factory。
- `src/qtllm/tools/`：工具定义、选择、执行、协议适配、MCP。
- `src/qtllm/toolsinside/`：运行观测和 trace 查询。
- `src/qtllm/toolsstudio/`：工具目录和工作区管理。
- `src/qtllm/logging/`：日志和信号 sink。

## 分层原则

UI 只组织交互，不承载 provider 细节；Host facade 解释外部配置；client 层统一执行请求；provider 层处理协议差异；runtime 层处理本地进程和模型；observability 层记录运行过程。
