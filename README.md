# qt-llm

`qt-llm` is a lightweight Qt-based LLM interface layer for desktop applications. It provides a practical Qt/C++ foundation for building products with built-in support for LLM conversation, Tools, MCP, SKILL, and Agent workflows.

The project is designed for direct code-level integration into a user's own Qt application. It keeps the reusable core, practical tool apps, and business-agent references clearly separated so developers can adopt only the layers they need.

## Project Purpose

The purpose of this project is to provide:

- a lightweight Qt-oriented LLM interface layer
- built-in support for LLM conversation, Tools, MCP, SKILL, and Agent development
- direct C++/Qt integration into a user's own project
- runtime monitoring, tracing, and analysis for LLM workflows

This repository is not positioned as a web-first orchestration service. It is positioned as a Qt/C++ integration base that can be embedded into desktop products and internal tools with a low adoption barrier.

## Three Layers

The repository follows a three-part top-level structure.

### 1. `qtllm` (`src/qtllm/`)

This is the core layer.

It provides reusable infrastructure for:

- provider abstraction
- request/response execution
- streaming response handling
- conversation management
- tool runtime
- MCP integration
- storage
- logging and tracing
- runtime observability support

### 2. `apps` (`src/apps/`)

This is the practical tools layer.

It contains host and utility applications built on top of the core layer, including examples such as chat hosts, MCP tools, and `toolsinside`-style analysis utilities.

These apps are useful in their own right, but they also serve as reference hosts for how to build on `qtllm`.

### 3. `agents` (`src/agents/`)

This is the business-agent layer.

It is used to build common Agents and to provide reference implementations for Agent development. These applications validate how to build workflow-oriented products on top of the shared core instead of adding business logic into `src/apps/`.

The current reference agent is:

- `src/agents/pdf_translator_agent/`

## Integration Style

The intended usage style is simple:

- include and link the `qtllm` library in a Qt project
- configure providers and runtime capabilities in C++
- compose conversation, tool, MCP, SKILL, and Agent flows inside the user's own UI and application logic

The design goal is to make code-level integration concise and practical for Qt developers.

## Monitoring, Tracing, and Analysis

A major feature of the project is that it does not stop at request execution.

It also emphasizes runtime monitoring, tracing, and analysis for:

- LLM conversation flows
- tool execution flows
- MCP calls
- SKILL execution paths
- Agent workflow stages

The repository already includes supporting infrastructure and reference surfaces for:

- structured logs
- runtime traces
- artifact recording
- workflow state inspection
- analysis-oriented utilities such as `toolsinside`

This makes the project suitable not only for building LLM functions, but also for understanding, debugging, and improving them over time.

## Identity and ID Rules

The repository now treats ID design as a top-level engineering constraint rather than a local storage detail.

- business and observability IDs should use compact prefixed IDs instead of GUID/UUID strings
- public interfaces should continue to expose IDs as `QString`
- internal persistence may be reinitialized when the ID architecture changes
- new code should use the centralized identity module under `src/qtllm/identity/`

The current canonical prefixes cover core runtime, tracing, ToolStudio, and agent workflow entities such as `cli`, `ses`, `trc`, `req`, `spn`, `evt`, `tcl`, `art`, `lnk`, `wsp`, `nod`, `plc`, `pkg`, `tsk`, and `que`.

Detailed rationale and rules are documented in:

- [docs/IDENTITY_ARCHITECTURE.md](./docs/IDENTITY_ARCHITECTURE.md)
- [CODING_GUIDELINES.md](./CODING_GUIDELINES.md)
- [docs/DECISIONS.md](./docs/DECISIONS.md)

## Typical Usage

Typical usage scenarios include:

- embedding LLM chat into a Qt desktop application
- adding tool-calling and MCP-backed capability to an existing Qt product
- building internal operational tools around LLM workflows
- building domain-specific Agents with task pipelines, persisted state, and UI

## Environment and Technology Stack

- language: C++17
- framework: Qt
- build baseline: qmake
- target shape: lightweight desktop integration
- current primary validation environment: Windows
- intended runtime targets: Windows and Linux

## Recommended Reading Order

1. [README.md](./README.md)
2. [docs/PROJECT_INTRODUCTION.md](./docs/PROJECT_INTRODUCTION.md)
3. [docs/REPOSITORY_STRUCTURE.md](./docs/REPOSITORY_STRUCTURE.md)
4. [docs/ROADMAP.md](./docs/ROADMAP.md)
5. [docs/IDENTITY_ARCHITECTURE.md](./docs/IDENTITY_ARCHITECTURE.md)
6. [docs/developer-guide/README.md](./docs/developer-guide/README.md)
7. [docs/agents/README.md](./docs/agents/README.md)

## Code Entry Points

- core library: `src/qtllm/`
- host and utility apps: `src/apps/`
- business agents: `src/agents/`
- tests: `tests/qtllm_tests/`
- docs: `docs/`
