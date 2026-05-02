# 运行数据布局

## llama.cpp runtime

```text
llama-cpp-runtime/
  bin/
  models/
  logs/
```

## ToolsInside

默认 artifact 路径位于 `.qtllm/tools_inside/artifacts`。trace 数据按 client、session、trace 组织。

## 会话快照

`ConversationSnapshot` 保存：

- client id
- LLM config
- profile
- active session id
- sessions
- 每个 session 的 history

旧的单 history 格式有兼容读取路径。
