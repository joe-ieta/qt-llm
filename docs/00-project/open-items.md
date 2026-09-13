# 当前待办

基础能力优化工作包 QTL-00 至 QTL-10 已完成。以下内容是后续增强项，不影响当前公开接口和发布矩阵结论。

## 工程与平台

- 在 Linux 和 macOS 建立与 Windows 等级一致的 Qt5/Qt6 构建与安装包验证。
- 评估逐步显式标注全部 Windows 公共 ABI，最终移除 `WINDOWS_EXPORT_ALL_SYMBOLS` 兼容层。
- 发布前确定下一版本号，并同步 CMake 版本、发布说明、标签和归档名称。

## 外部运行环境

- 在可用的真实 MCP stdio 与 HTTP-like server 环境中补充端到端冒烟。
- 在真实 OpenAI-compatible、Ollama、vLLM 和 llama.cpp 服务上维护可选集成测试。
- 增加 llama.cpp runtime 下载、校验、安装、升级和故障修复工具。

## 上层迁移

- CoReader 按独立工作包迁移到正式 CMake 目标和推荐请求入口。
- CoReader 的业务提示词、文档处理、RAG、任务编排和 UI 继续保留在宿主侧。
