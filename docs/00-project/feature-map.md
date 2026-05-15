# 功能清单

## 已实现

- `QtLLMClient`：统一 LLM 请求入口，支持普通请求、流式 token、取消请求、payload 观测。
- provider：OpenAI、OpenAI-compatible、Ollama、vLLM、llama.cpp。
- `RuntimeFacade`：面向 Host App 的 RuntimeProfile 映射、模型发现、阻塞/非阻塞请求。
- 内置 `llama.cpp` 托管：运行态目录发现、`llama-server` 启动、端口等待、本地 `.gguf` 模型发现、基于模型和本机能力的自动运行规划。
- provider 可用性字段：`providerAvailable`、`providerAvailabilityStatus`、`providerAvailabilityMessage`、`resolvedRuntimeRoot`、`resolvedModelPath`、`localModelCount`。
- `ConversationClient`：多会话、历史、profile、snapshot、持久化工厂。
- 工具调用：工具定义、工具选择、执行层、协议适配和调用编排。
- MCP：server registry、repository、manager、tool sync、默认 client。
- ToolsInside：trace、timeline、span、artifact、tool call、支持链接查询。
- ToolStudio：工具目录、工作区、导入导出、合并和 metadata override。
- 紧凑 ID：client、session、trace、request、span、event、tool call、artifact 等前缀 ID。
- 示例 App 和参考 Agent。

## 部分实现

- Host App 统一集成路径已经建立，更多示例仍需逐步迁移到 `RuntimeFacade`。
- 本地 `llama.cpp` 已支持自动发现和可用性检测，运行态下载/自动安装仍需后续增强。
- 观测数据结构已具备，跨 App 的统一可视化体验仍可继续打磨。

## 计划中

- 更完整的 RuntimeFacade 示例和 Host App 迁移指南。
- 本地 runtime 的安装、升级、校验和诊断工具。
- provider 配置 UI 与可用性状态的统一展示。
- 发布包中的 runtime 布局和模型目录约定进一步固化。
