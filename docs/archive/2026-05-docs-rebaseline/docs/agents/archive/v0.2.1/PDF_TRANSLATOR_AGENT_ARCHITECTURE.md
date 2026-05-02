# PDF Translator Agent Architecture

## 1. Positioning

`PDF Translator Agent` is a new business-agent desktop application that will live outside `src/apps/` and reuse `src/qtllm/` as its infrastructure layer.

This agent is positioned as:
- a human-in-the-loop product application
- focused on PDF translation only
- suitable for non-technical users
- able to run on Windows and Linux
- able to use different models and MCP backends for different skills

This agent is not:
- a new generic library inside `src/qtllm/`
- an extension of the current demo/host apps in `src/apps/`
- a one-shot script tool
- an OCR-first scanning workflow product in phase 1

## 2. Product Goals

The first productized scope is:
- fragment translation with multi-turn refinement
- single-document PDF translation
- batch background translation
- side-by-side reading of original and translated content
- artifact persistence on disk
- MCP integration from phase 1

The first scope is limited to text-extractable PDFs. Scanned PDFs are outside the mainline flow, but OCR remains a pluggable MCP fallback path.

## 3. Repository Placement

The agent must be implemented under a new top-level business-agent area.

Recommended location:

```text
src/agents/pdf_translator_agent/
```

Recommended subdirectories:

```text
src/agents/pdf_translator_agent/
  app/
  ui/
  domain/
  pipeline/
  skills/
    core/
    builtin/
    mcp/
  storage/
  viewer/
```

The intent is to make `src/apps/` remain infrastructure/demo hosts while `src/agents/` becomes the business-agent product area.

## 4. Technical Baseline

- language: C++17
- framework: Qt
- UI stack: Qt Widgets
- build system: qmake
- runtime targets: Windows and Linux

The viewer path is phased:

### Phase 1 viewer baseline

- Qt Widgets shell
- original document shown as extracted and structured text
- translated content shown as generated HTML/Markdown-backed reading view
- paragraph/chunk anchor linking for compare reading

### Phase 2 viewer enhancement

- reader-grade page rendering
- evaluate `QWebEngineView + pdf.js` for original PDF page rendering
- keep the agent shell and orchestration in Qt Widgets

## 5. Reused Infrastructure from `qtllm`

The new agent should reuse:
- `QtLLMClient` for unified provider access
- `ConversationClient` for fragment translation and refinement turns
- provider system for OpenAI / OpenAI-compatible / Ollama / vLLM routing
- MCP integration for OCR, terminology, and document-analysis tools
- logging and `toolsinside` for traceability

The new agent should not move business workflow logic into:
- `src/qtllm/core`
- `src/qtllm/providers`
- `src/qtllm/tools`
- existing `src/apps/*`

## 6. Product Modes

### 6.1 Fragment Translation

User interaction model:
- chat-like panel
- paste or type a text fragment
- auto-detect source language
- translate to the opposite language in current phase
- allow follow-up corrections and re-translation

Recommended backend path:
- `ConversationClient`
- `language-detect` skill
- `fragment-translate` skill

### 6.2 Single Document Translation

User interaction model:
- choose one PDF
- show original on one side and translation on the other side
- show extraction and translation progress
- allow restart, partial retry, and open output files

Recommended backend path:
- document import
- text extraction
- language detection
- chunking
- chunk translation
- quality checks
- output assembly

### 6.3 Batch Background Translation

User interaction model:
- choose multiple PDFs
- run in background
- show per-document progress, status, and failure reason
- allow pause, retry, cancel, and resume
- allow compare-reading after completion

Recommended backend path:
- batch queue
- per-document task state
- bounded concurrency
- incremental persistence

## 7. Agent Runtime Model

This product should be implemented as a structured workflow agent, not as an unconstrained free-form chatbot.

The agent runtime should be divided into:
- workflow orchestration
- skill execution
- model routing
- MCP integration
- artifact persistence
- UI progress/reporting

The primary flow is deterministic in structure, even when LLM-backed skills are used internally.

## 8. Skill Framework

### 8.1 Skill Positioning

In this product, a skill is a runtime business capability unit. A skill is not the same thing as:
- a Codex development skill
- an MCP tool
- a free-form prompt template

A skill owns a narrow responsibility, can be scheduled by workflow stage, and may execute through:
- local rules
- local code
- LLM calls
- MCP tools
- hybrid flows

### 8.2 Skill Categories

The initial categories are:
- `analysis`
- `preprocess`
- `translation`
- `quality`
- `assembly`

### 8.3 Core Skill Types

Recommended core abstractions:
- `SkillDescriptor`
- `SkillContext`
- `SkillResult`
- `ISkill`
- `SkillRegistry`
- `SkillOrchestrator`

### 8.4 Initial Skill Set

Phase-1 target skills:
- `language-detect`
- `pdf-text-extract`
- `document-structure-normalize`
- `chunk-translate`
- `translation-qa`
- `document-assemble`

The first validation skill is `language-detect`.

## 9. Multi-Model Routing

The agent must support different model or backend bindings per skill.

This is a first-class requirement, because:
- OCR-related tasks may need a different backend from translation
- language detection should prefer a cheaper and faster path
- quality checking may use another model than the main translation model
- some skills may be MCP-first rather than LLM-first

### 9.1 Routing Layers

The routing model should support:
- global default endpoint
- per-skill endpoint binding
- per-task override
- fallback endpoint binding
- MCP tool routing where applicable

### 9.2 Suggested Core Objects

- `ModelEndpointConfig`
- `SkillExecutionBinding`
- `ModelRouter`

### 9.3 Execution Types

Each skill should declare an execution type:
- `builtin`
- `llm`
- `mcp`
- `hybrid`

Example intent:
- `language-detect`: `hybrid`
- `pdf-text-extract`: `builtin` or `mcp`
- `chunk-translate`: `llm`
- `translation-qa`: `llm`
- `ocr-fallback`: `mcp`

## 10. Task Pipeline

The document translation pipeline should be:

```text
import PDF
  -> extract text
  -> normalize structure
  -> detect source language
  -> choose target language
  -> chunk document
  -> translate chunks
  -> run QA checks
  -> assemble outputs
  -> generate compare-reading index
```

The batch pipeline should wrap the same document pipeline with queueing, scheduling, bounded concurrency, and resumability.

## 11. Threading and Concurrency Model

The product must not expose raw threading complexity to the user.

Recommended execution layout:

- UI main thread
  - widgets, status updates, viewer interactions
- document processing thread pool
  - text extraction, normalization, chunk preparation
- translation scheduler
  - queueing, retry, cancellation, pause/resume, rate limiting
- LLM/MCP execution workers
  - bounded concurrent requests
- persistence worker
  - save manifests, chunk outputs, and final artifacts

Concurrency rules:
- do not create unbounded per-document threads
- document-level concurrency must be configurable
- chunk-level concurrency must be conservative
- global concurrency must respect endpoint/backend limits

## 12. Storage Model

The product has two storage domains.

### 12.1 User-visible outputs

Stored next to the original PDF:
- translated Markdown
- translated HTML
- translation manifest JSON

Suggested naming:
- `file.translated.zh-CN.md`
- `file.translated.zh-CN.html`
- `file.translation.manifest.json`

### 12.2 Internal runtime state

Stored under an application workspace:
- task index
- queue state
- chunk cache
- retry state
- trace/artifact metadata
- settings

User document directories must not become the dumping ground for internal scheduler state.

## 13. Language Scope

Phase 1 language scope is:
- English to Chinese
- Chinese to English
- automatic source-language detection

Target language selection logic:
- if source is English, translate to `zh-CN`
- if source is Chinese, translate to English
- if uncertain, ask for user confirmation or apply configured default

## 14. Human Interaction Principles

This is a human-in-the-loop product.

The application should:
- keep the main workflow obvious
- expose simple actions for non-technical users
- surface progress and failure clearly
- allow user correction and restart at meaningful checkpoints
- keep advanced configuration hidden by default

The application should avoid exposing these concepts in the primary workflow:
- provider name
- base URL
- raw model identifiers
- MCP server internals
- trace IDs
- raw prompts

## 15. Reader Strategy

Reader-grade PDF display is a product goal, but not a phase-1 blocker.

Phase 1:
- structured-text original view
- translated reading view
- anchor-linked compare reading

Phase 2:
- evaluate embedded page renderer with `QWebEngineView`
- prefer mature page-rendering capability over ad hoc custom drawing

## 16. First Validation Skill: `language-detect`

Purpose:
- detect whether source text is Chinese or English
- return confidence and evidence
- recommend the opposite target language for phase 1

Execution strategy:
- first-pass local heuristic rules
- fallback to a lightweight bound model when confidence is low
- output normalized language codes

Suggested normalized result:

```json
{
  "sourceLanguage": "en",
  "targetLanguage": "zh-CN",
  "confidence": 0.94,
  "method": "rule"
}
```

or:

```json
{
  "sourceLanguage": "zh-CN",
  "targetLanguage": "en",
  "confidence": 0.78,
  "method": "llm"
}
```

## 17. Phase-1 Delivery Focus

Before expanding feature breadth, phase 1 should first stabilize:
- skill framework
- multi-model routing
- `language-detect` skill
- text-extractable PDF pipeline
- single-document translation
- batch queue baseline
- output persistence and compare-reading baseline

Reader-grade PDF rendering, OCR-heavy flows, and richer quality skills should be added after the phase-1 execution path is stable.
