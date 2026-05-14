# ToolStudio

ToolStudio 是工具管理和编辑参考应用。

主要能力：

- 工具目录管理。
- 工具导入导出。
- workspace/node/placement 管理。
- metadata override。
- MCP 工具同步后的人工整理。

ToolStudio 面向工具生态维护，不是普通聊天 Host App 的必选部分。

启动时，App 会尝试拉起 managed `llama-server`，方便工具调试和后续模型调用路径使用同一个本地 runtime。
