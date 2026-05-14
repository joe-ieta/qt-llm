# mcp_server_manager

`mcp_server_manager` 用于管理和调试 MCP server，并提供 MCP chat 相关参考界面。

主要能力：

- MCP server 扫描和发现。
- 手工添加 MCP server。
- MCP server 注册和配置管理。
- MCP chat 调试入口。

启动时，App 会尝试拉起 managed `llama-server`。如果本地 `llama-cpp-runtime` 或模型不可用，启动失败只记录日志，不阻断 MCP 管理界面。
