# Roadmap

## Phase 1: core baseline

- `QtLLMClient`
- provider abstraction
- HTTP executor
- base chat capability

## Phase 2: conversation and tools

- `ConversationClient` / `ConversationClientFactory`
- conversation persistence
- tool catalog, selection, execution, and failure guard

## Phase 3: MCP and observability

- MCP server registry, sync, and execution
- unified logging
- `toolsinside` trace and artifact recording

## Phase 4: asset and workspace management

- `toolstudio` workspaces
- import/export and merge
- editable tool metadata and placement organization

## Phase 5: business-agent validation

- validate independent business-agent development under `src/agents/`
- validate skill-oriented workflow composition over the shared `qtllm` base
- validate per-skill model routing and MCP-backed workflow integration

## Current Focus

- keep `src/qtllm` as reusable infrastructure
- keep `src/apps` as host/demo surfaces
- use `src/agents` for business-agent validation and future product applications
- reduce documentation drift between implementation state and historical design notes
