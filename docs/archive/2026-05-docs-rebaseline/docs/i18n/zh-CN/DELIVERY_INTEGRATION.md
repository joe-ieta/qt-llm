# Delivery and Integration / 交付与集成说明

## Provider 映射
- `openai` -> `OpenAIProvider`
- `openai-compatible` -> `OpenAICompatibleProvider`
- `ollama` -> `OllamaProvider`
- `vllm` -> `VllmProvider`
- `sglang`、`anthropic`、`google`、`gemini`、`deepseek`、`qwen`、`glm`、`zhipu` -> `OpenAICompatibleProvider`

## 集成说明
- 核心库需要 `QT += core network`
- 使用示例风格界面时增加 `widgets`
- MCP Server 持久化到工作区 `.qtllm/mcp/servers.json`
- 日志持久化到工作区 `.qtllm/logs/<clientId>/`
