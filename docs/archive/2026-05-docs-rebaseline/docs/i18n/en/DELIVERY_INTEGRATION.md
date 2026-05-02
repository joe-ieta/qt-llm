# Delivery and Integration / Delivery Integration

## Provider Mapping
- `openai` -> `OpenAIProvider`
- `openai-compatible` -> `OpenAICompatibleProvider`
- `ollama` -> `OllamaProvider`
- `vllm` -> `VllmProvider`
- vendor aliases such as `sglang`, `anthropic`, `google`, `gemini`, `deepseek`, `qwen`, `glm`, `zhipu` -> `OpenAICompatibleProvider`

## Integration Notes
- core library requires `QT += core network`
- add `widgets` for example-style UI integration
- MCP servers persist under workspace `.qtllm/mcp/servers.json`
- logs persist under workspace `.qtllm/logs/<clientId>/`
