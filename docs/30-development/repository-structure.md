# 仓库结构

```text
qt-llm/
  qt-llm.pro
  src/
    qtllm/
    apps/
    agents/
  tests/
    qtllm_tests/
  docs/
    00-project/
    10-architecture/
    20-integration/
    30-development/
    40-apps/
    50-reference/
    releases/
    archive/
```

## src/qtllm

核心库。所有可复用的 LLM、provider、runtime、chat、tool、MCP、logging、ToolsInside、ToolStudio 逻辑应优先放在这里。

## src/apps

示例和工具应用。它们可以展示推荐用法，但不应承载只能被核心库复用的逻辑。

## src/agents

业务 Agent 参考实现。Agent 可以验证工作流型产品如何使用核心库，但业务逻辑不应反向污染核心库。
