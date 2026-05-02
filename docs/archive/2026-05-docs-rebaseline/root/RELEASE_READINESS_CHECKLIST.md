# RELEASE_READINESS_CHECKLIST.md

## Scope

This checklist targets the current qt-llm baseline after:
- multi-client factory and session persistence
- tool runtime abstraction with built-in tools
- MCP server registry, persistence, sync, and execution routing
- dedicated OpenAI and OpenAI-compatible provider paths
- unified runtime logging with file rotation and UI delivery

## 1. Build and Packaging
- [ ] `nmake` completes successfully for library, tests, and examples.
- [ ] `simple_chat` starts and sends a basic request.
- [ ] `multi_client_chat` starts and core UI interactions are stable.
- [ ] `mcp_server_manager_demo` starts and both management/chat windows are usable.
- [ ] No new mandatory external dependency was introduced.

## 2. Core Chat Regression
- [ ] Base chat path works without tool entry (`ConversationClient -> QtLLMClient`).
- [ ] Streaming content and reasoning deltas arrive in order.
- [ ] Final completion is emitted once per request.
- [ ] Error path emits `errorOccurred` and does not deadlock future requests.

## 3. Multi-Client / Session Persistence
- [ ] New client creation generates unique `clientId`.
- [ ] Re-acquiring an existing `clientId` restores config, profile, and history.
- [ ] Per client, only one active session is used at a time.
- [ ] Finished dialogue can be archived by creating a new session.
- [ ] Session switching displays the correct historical messages.

## 4. Tool Registry and Policy
- [ ] Built-in tools are auto-injected on startup.
- [ ] Built-in tools remain non-removable and enabled.
- [ ] `toolId`, `invocationName`, and display `name` remain consistent.
- [ ] Client tool policy file persists under `.qtllm/clients/<clientId>/tool_policy.json`.

## 5. MCP Registry and Sync
- [ ] MCP server file persists under `.qtllm/mcp/servers.json`.
- [ ] Invalid MCP entries are rejected with actionable errors.
- [ ] Registered MCP servers can be reloaded across app restarts.
- [ ] MCP tool sync imports tools into the shared registry.
- [ ] MCP tool execution routes by server source and returns structured errors on failure.

## 6. Tool Runtime Behavior
- [ ] Tool selection layer can filter tools for the current turn.
- [ ] Execution layer enforces allow/deny policy and per-turn limits.
- [ ] Default dry-run or missing-executor paths return structured failure without crash.
- [ ] Internal loop stops after failure guard threshold.
- [ ] Loop context uses `clientId + sessionId` correctly.

## 7. Provider Protocol Mapping
- [ ] OpenAI path builds `/responses` and parses text plus function-call events.
- [ ] OpenAI-compatible path builds `/chat/completions` and parses `tool_calls`.
- [ ] Anthropic mapping builds `/messages` with expected tool blocks.
- [ ] Google mapping builds `...:generateContent` with expected function-call parts.
- [ ] Provider aliases resolve to the expected provider implementations.

## 8. Observability and Failure Diagnostics
- [ ] Logs are written under `.qtllm/logs/<clientId>/`.
- [ ] File rotation and per-client separation work under repeated requests.
- [ ] `SignalLogSink` can stream logs into example UIs.
- [ ] Errors contain actionable provider, tool, or MCP context.
- [ ] Request/session/client identifiers are present where applicable.

## 9. Security and Safety Baseline
- [ ] API keys are not written to logs.
- [ ] Tool policy defaults do not unintentionally widen permissions.
- [ ] External tool executors and MCP requests have timeout handling.
- [ ] Invalid MCP server configuration cannot silently broaden access.

## 10. Test Coverage Gates
- [ ] Unit tests for provider mapping pass.
- [ ] Existing core tests pass without flaky failures.
- [ ] Manual smoke tests completed for all example apps.

## 11. Known Remaining Work
- [ ] Broader automated coverage for MCP client transports and OpenAI Responses API edge cases.
- [ ] Structured tool trace persistence inside session history.
- [ ] Richer structured log viewers and export helpers.
- [ ] Production-grade asynchronous external tool execution model.
