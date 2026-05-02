# PDF Translator Agent Status

## Purpose

`PDF Translator Agent` is the current business-agent validation application under:

`src/agents/pdf_translator_agent/`

Its present role is:

- validate that `qtllm` can support business-agent development outside `src/apps/`
- validate skill-oriented workflow composition on top of the shared LLM and MCP base
- validate a desktop agent shell with fragment, document, and batch workflows

It is not yet the final form of a production PDF translation product.

## Repository Position

- `src/qtllm/`
  - reusable infrastructure library
- `src/apps/`
  - infrastructure and host/demo applications
- `src/agents/pdf_translator_agent/`
  - business-agent validation application

This separation is now implemented in code and should remain the default repository rule for future business agents.

## Functional Coverage

### Implemented

- fragment translation
- single-document translation
- batch background translation
- side-by-side compare reading using extracted original text plus translated Markdown/HTML
- per-skill backend binding
- MCP-backed document parsing fallback pattern
- MCP-backed terminology enhancement pattern
- persisted manifests and translated artifacts
- persisted UI state and batch queue state

### Still Limited

- original-document reading is not page-grade PDF rendering
- OCR is not yet a dedicated runtime skill
- Linux build/runtime validation is pending
- translated JSON export is not yet a first-class output artifact

## Module Layout

### Application Shell

- `app/`
  - bootstrap and skill wiring
- `ui/`
  - Qt Widgets application shell and page composition

### Domain

- `domain/documenttranslationtask.*`
  - document task state, JSON serialization, and stage transitions

### Pipeline

- `pipeline/documentworkflowcontroller.*`
  - single-document workflow orchestration
- `pipeline/batchtranslationqueuecontroller.*`
  - bounded-concurrency batch queue, retry, pause, restore, and progress fan-out

### Skills

- `skills/core/`
  - `ISkill`, descriptors, skill registry, execution bindings, and model router
- `skills/builtin/`
  - current built-in runtime skills
- `skills/mcp/`
  - MCP gateway wrapper for agent-side tool usage

### Storage

- `storage/manifestrepository.*`
  - manifest save/load path

### Reading

- `viewer/comparereaderwidget.*`
  - compare-reading widget for extracted original text and translated output

## Current Skill Set

### Implemented Skills

- `language-detect`
  - rule-first, LLM-fallback language detection
- `pdf-text-extract`
  - local tool first, MCP fallback pattern
- `term-base-resolve`
  - MCP terminology enrichment
- `chunk-translate`
  - main translation skill with support for revision instructions
- `document-assemble`
  - Markdown/HTML artifact generation

### Not Yet Implemented as Dedicated Skills

- OCR fallback as an explicit runtime skill
- structure normalization skill
- translation QA skill

## Runtime Interface Summary

### UI-to-Pipeline

- fragment page triggers `language-detect` and `chunk-translate`
- document page triggers `DocumentWorkflowController`
- batch page triggers `BatchTranslationQueueController`

### Pipeline-to-Skills

Current single-document pipeline:

`extract -> detect-language -> term-base(optional) -> translate -> assemble`

The broader design target still includes normalization and QA, but those are not yet separate runtime stages.

### Model Routing

Current routing supports:

- skill-specific endpoint registration
- skill-specific execution binding
- LLM and MCP execution-type separation

Configured in practice for:

- `language-detect`
- `chunk-translate`
- `pdf-text-extract`
- `term-base-resolve`

### Batch Runtime Events

Current batch runtime exposes:

- queue changes
- active-task changes
- paused state changes
- active-job count changes
- per-task progress changes

This is sufficient for the current UI, but still UI-driven rather than formalized as a cross-cutting workflow event layer.

## Storage Specification

### User-Visible Output Files

Stored beside the source PDF:

- `*.translated.<lang>.md`
- `*.translated.<lang>.html`
- `*.translation.manifest.json`
- `*.extracted.txt`

### Internal Application State

Currently persisted through `QSettings`:

- endpoint and MCP configuration
- fragment UI state and history
- batch queue state
- current tab and selected runtime options

This is acceptable for the current validation stage, but a dedicated settings/state repository would be cleaner in the next iteration.

## Logging and Observability

### Current State

- task stages are visible in UI
- manifests are written to disk
- output files are deterministic and inspectable
- batch progress updates are surfaced in real time

### Current Limitation

The implementation uses the shared `qtllm` runtime stack, but the agent itself does not yet provide a dedicated structured event log or a first-class trace viewer integration for its own workflow events.

For the current validation phase, observability is present but still basic.

## Document Status

### Current Source of Truth

- this file
- `PDF_TRANSLATOR_AGENT_DELIVERY_CHECKLIST.md`

### Historical Design Documents

The original architecture and scope-freeze documents have been archived under:

- `docs/agents/archive/v0.2.1/`

They remain valid as design history, but they should not be edited as the primary implementation record going forward.

## Recommended Documentation Rule Going Forward

For this agent area, keep only:

- one current status document
- one current delivery/gap checklist
- per-version release notes
- archived historical design documents by version

This keeps the active document surface small and lowers maintenance overhead during future iterations.
