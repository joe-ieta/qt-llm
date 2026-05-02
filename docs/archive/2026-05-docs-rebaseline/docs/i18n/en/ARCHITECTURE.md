# Architecture Mirror

Canonical architecture document:
- [../../../ARCHITECTURE.md](../../../ARCHITECTURE.md)

Current architecture summary:
- `src/qtllm/` is the project center
- `src/apps/` hosts integration surfaces for chat, MCP, trace analysis, and tool management
- the main runtime chain is `ToolEnabledChatEntry / ConversationClient -> QtLLMClient -> Provider -> HttpExecutor`
- `toolsinside` and `toolstudio` are implemented subsystems, not design-only placeholders
