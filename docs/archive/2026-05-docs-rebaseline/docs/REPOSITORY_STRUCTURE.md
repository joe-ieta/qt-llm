# Repository Structure

## Top Level

- `README.md`
  - repository entry point
- `qt-llm.pro`
  - qmake workspace entry
- `docs/`
  - project, developer, release, and agent documentation
- `src/`
  - library, apps, and business agents
- `tests/`
  - automated tests

## Reusable Library

- `src/qtllm/`
  - reusable Qt/C++ LLM integration library
  - core request flow, provider layer, storage, tools, MCP, logging, and observability support

## Host and Demo Applications

- `src/apps/`
  - infrastructure and demo hosts built on top of `qtllm`
  - current examples include chat, MCP management, tools-inside, and toolstudio applications

## Business Agents

- `src/agents/`
  - business-agent applications built on the shared library layer

- `src/agents/pdf_translator_agent/`
  - current validation agent for:
    - skill-based workflow composition
    - per-skill model routing
    - MCP-backed business-agent integration
    - fragment, document, and batch task flows

## Tests

- `tests/qtllm_tests/`
  - core library test baseline

## Documentation

- `docs/PROJECT_INTRODUCTION.md`
  - high-level repository positioning
- `docs/DELIVERY_INTEGRATION.md`
  - delivery and integration notes
- `docs/TESTING_BASELINE.md`
  - current testing expectations
- `docs/ROADMAP.md`
  - current phased direction
- `docs/DECISIONS.md`
  - architectural decisions
- `docs/IDENTITY_ARCHITECTURE.md`
  - repository-wide ID model, constraints, and migration plan
- `docs/developer-guide/`
  - developer-facing guide for `qtllm`
- `docs/agents/`
  - current business-agent status, checklists, release notes, and archived design docs
- `docs/releases/`
  - repository release note archive
- `docs/i18n/`
  - translated document mirrors, no longer the primary entry point

## Runtime Workspace

The repository writes runtime state under `.qtllm/`, including:

- `.qtllm/clients/`
- `.qtllm/mcp/`
- `.qtllm/tools/`
- `.qtllm/logs/`
- `.qtllm/tools_inside/`
