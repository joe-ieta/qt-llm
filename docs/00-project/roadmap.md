# 路线图

## 当前重点

1. 稳定 Host App 集成路径。
2. 完善本地 `llama.cpp` 托管体验。
3. 强化 provider 可用性、模型发现和错误诊断。
4. 保持工具调用、MCP 和 ToolsInside 的架构边界清晰。
5. 让示例 App 能清楚展示推荐用法。

## 近期方向

- 将 simple_chat、multi_client_chat 中的集成方式保持为推荐示例。
- 继续补充 RuntimeFacade 的模型列表、运行状态和错误状态使用样例。
- 为本地运行态增加更明确的目录检查和用户提示。
- 补齐 Linux 运行和打包说明。

## 中期方向

- 将外部 Host App 集成稳定为版本化公共接口。
- 改善 ToolStudio、ToolsInside 与工具调用运行时之间的闭环。
- 提供更系统的 runtime 和模型目录管理能力。
- 形成更完整的 release checklist 和发布说明模板。
