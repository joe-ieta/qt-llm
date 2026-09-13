# 仓库结构

```text
qt-llm/
  CMakeLists.txt
  cmake/                 安装包配置模板
  scripts/               文档与发布验证工具
  src/
    qtllm/               可复用基础库
    apps/                示例和工具应用
    agents/              参考业务 Agent
  tests/
    qtllm_tests/         行为测试
    core_component/      轻量核心测试
    optional_components/ 组件测试
    consumers/           安装包与源码集成测试
  docs/
    00-project/          定位、状态、决策和路线
    10-architecture/     架构与边界
    20-integration/      外部开发接口和集成手册
    30-development/      构建、测试和发布
    40-apps/             示例 App 与参考 Agent
    50-reference/        API、配置、布局和故障参考
    releases/            当前发布与未发布说明
    archive/             历史文档
```

## 边界

- `src/qtllm` 不包含文档阅读、翻译、RAG、工作流或其他宿主业务逻辑。
- `src/apps` 展示推荐用法，不承载只能由基础库复用的实现。
- `src/agents` 可以组合基础能力，但业务实现不反向进入 `src/qtllm`。
- `tests/consumers` 验证外部工程实际看到的 CMake 目标、头文件和二进制契约。
- `docs/archive` 不作为当前设计和集成依据。
