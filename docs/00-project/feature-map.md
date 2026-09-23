# 功能清单

## 已实现并验证

- 统一请求类型、流式增量、请求 ID、结构化错误、唯一终态、取消、受控重试和带可用性标记的 Provider token 用量。
- `RuntimeFacade::sendAsync()` 与独立 `RuntimeRequestHandle`，支持并行请求和精确取消。
- `QtLLMClient` 高级请求入口及原有兼容信号。
- OpenAI、OpenAI-compatible、Ollama、vLLM 和 llama.cpp Provider。
- JSON/JSON Schema 结构化输出、受控 Schema 子集和模型能力快照。
- 消息数与 token 双预算的上下文窗口，支持注入精确 tokenizer。
- `ConversationClient` 多会话、历史、快照、宿主自管历史和可选持久化。
- 工具注册、选择、授权、顺序/受控并发、超时、取消、重试和 Internal/External 执行模式。
- MCP server 注册、持久化、工具同步及 stdio/HTTP-like 客户端路径。
- 托管 llama.cpp 发现、模型目录、运行规划、健康检查、共享实例、租约和排队。
- 可注入 LLM 事件接收端、日志、ToolsInside 和 ToolStudio。
- `Core`、`Diagnostics`、`Tools`、`LocalRuntime`、`Conversation` 组件及兼容聚合目标。
- Qt5/Qt6、STATIC/SHARED、安装包和源码子目录集成。

以上构建与集成能力已由发布验证门禁覆盖。真实模型服务和依赖外部 MCP 服务的测试仍受运行环境约束。

## 可继续增强

- Linux/macOS 的同等级自动构建和安装包矩阵。
- llama.cpp runtime 的下载、签名校验、安装和升级工具。
- Provider 配置 UI 与可用性状态的统一参考组件。
- 更多真实 MCP server、远端 Provider 和 GPU runtime 冒烟环境。
- 逐步用显式导出覆盖全部公共 ABI，最终移除 Windows 自动导出兼容层。

## 明确不属于基础库

- 文档阅读、翻译、RAG、工作流 DAG 和业务 Agent 规则。
- 宿主 UI、账号、权限产品流程和业务数据库。
- 模型输出的业务正确性判断。
- 默认执行任意宿主进程或脚本。
