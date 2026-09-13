# 路线图

## 当前基线

QTL-00 至 QTL-10 已完成。当前基线提供稳定的请求生命周期、异步句柄、结构化输出、上下文预算、工具/MCP、本地运行实例、事件诊断和组件化 CMake 集成，并通过 Windows Qt5/Qt6、STATIC/SHARED 发布矩阵。

## 近期方向

- 推进 CoReader 等外部项目采用正式 CMake 组件和推荐入口。
- 增加真实 Provider、MCP server 和 GPU llama.cpp runtime 的可选端到端验证。
- 完善 runtime 安装、升级、完整性校验和诊断工具。
- 在不破坏兼容接口的前提下扩大显式 Windows ABI 导出覆盖。

## 中期方向

- 建立 Linux/macOS 的持续构建、安装和消费矩阵。
- 提供更统一但保持可选的 Provider 配置与运行状态参考 UI。
- 完善 ToolStudio、ToolsInside 与外部工具执行的调试闭环。
- 按语义化版本管理公共接口弃用、迁移和发布证据。
