# 运行时数据布局

## 1. 总体目录

当前工程默认把运行时数据写入工作区根目录下的 `.qtllm/`。

典型布局：

```text
.qtllm/
  clients/
  logs/
  mcp/
  tools/
  tools_inside/
```

## 2. `clients/`

用途：

- 保存 `ConversationSnapshot`

典型文件：

```text
.qtllm/clients/
  assistant-main.json
  planner.json
```

内容通常包括：

- config
- profile
- activeSessionId
- sessions

## 3. `logs/`

用途：

- 保存运行日志

典型布局：

```text
.qtllm/logs/
  assistant-main/
    20260320-0001.jsonl
```

特点：

- 按 client 切目录
- JSONL
- 支持轮转

## 4. `mcp/`

用途：

- 保存 MCP server 定义

典型文件：

```text
.qtllm/mcp/
  servers.json
```

## 5. `tools/`

用途：

- 保存工具目录和 Tool Studio 相关数据

可能包括：

- tool catalog
- metadata override
- workspace index
- workspaces

## 6. `tools_inside/`

用途：

- 保存 trace 级分析数据

典型布局：

```text
.qtllm/tools_inside/
  index.db
  artifacts/
  archive/
```

说明：

- `index.db` 保存结构化索引
- `artifacts/` 保存大对象内容
- `archive/` 保存归档内容

## 7. 开发建议

- 不同工作区最好使用明确的 workspace root
- 调试期间可以直接检查这些目录确认是否落盘
- 正式应用最好把工作区路径做成配置项，而不是完全依赖当前目录
