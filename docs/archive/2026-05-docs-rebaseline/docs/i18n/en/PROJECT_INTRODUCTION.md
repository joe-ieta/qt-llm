# Project Introduction

`qt-llm` is a lightweight Qt-based LLM interface layer for desktop applications. Its purpose is to provide a practical Qt/C++ foundation for building LLM-enabled products with built-in support for:

- LLM conversation
- Tools
- MCP
- SKILL
- Agent workflows

The project is designed to be integrated into a user's own Qt application at code level, with a compact API surface and a repository layout that keeps reusable infrastructure separate from host tools and business agents.

## Core Positioning

This project is not only a chat demo and not only a provider wrapper.

Its main goal is to offer a lightweight Qt-oriented interface layer that allows developers to:

- connect multiple LLM backends behind a consistent API
- build multi-turn conversation flows
- run tool-calling workflows
- integrate MCP servers and MCP-backed tools
- organize SKILL-style capability units inside products
- build workflow-oriented Agents on top of the same base

## Three Layers

The repository now follows a three-part top-level structure.

### 1. `qtllm` (`src/qtllm/`)

This is the core layer.

It provides the reusable infrastructure for:

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

It contains host and utility applications built on top of the core layer, for example:

- chat-oriented host applications
- MCP management tools
- `toolsinside` tracing and analysis tools
- tool and workspace management applications

These apps are meant to be useful on their own, but they also act as reference hosts for how to build on `qtllm`.

### 3. `agents` (`src/agents/`)

This is the business-agent layer.

It is used to build common Agents and to provide reference implementations for Agent development. These applications are not infrastructure demos; they validate how to build workflow-oriented products on top of the shared core.

The current reference agent is:

- `src/agents/pdf_translator_agent/`

## Integration Style

The project is intended to be embedded into a user's Qt project at code level.

The expected usage style is:

- include and link the `qtllm` library
- configure providers and runtime capabilities in C++
- compose conversation, tool, MCP, SKILL, and Agent flows inside the user's own Qt application

The design goal is to keep this integration simple and direct for Qt developers rather than forcing a separate service architecture first.

## Observability and Analysis

A major feature of the project is that it does not stop at request execution.

It also emphasizes runtime monitoring, traceability, and analysis for:

- LLM conversations
- tool execution
- MCP calls
- SKILL execution paths
- Agent workflow stages

The repository already includes support and reference surfaces for:

- structured logs
- runtime traces
- artifact recording
- workflow state inspection
- analysis-oriented utility applications such as `toolsinside`

This makes the project suitable not only for building LLM features, but also for understanding, debugging, and improving them over time.

## Typical Usage Scenarios

Typical usage scenarios include:

- embedding LLM chat into a Qt desktop product
- adding tool-calling and MCP-backed capability to an existing Qt application
- building internal operational tools around LLM workflows
- building domain-specific Agents with task pipelines, persisted state, and UI

## Environment and Technology Stack

- language: C++17
- framework: Qt
- build baseline: qmake
- target shape: lightweight desktop integration
- current primary validation environment: Windows
- intended runtime targets: Windows and Linux

The project uses Qt as the primary application and UI foundation rather than wrapping a web-first stack.
