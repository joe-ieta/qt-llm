# PDF Translator Agent Development Guardrails

## 1. Purpose

This document freezes the development boundaries for `PDF Translator Agent` before implementation begins.

Its role is to reduce feature drift, architecture drift, and accidental coupling with the existing infrastructure/demo apps.

## 2. Scope Freeze

The phase-1 implementation scope is strictly limited to:
- text-extractable PDF input
- fragment translation
- single-document translation
- batch background translation
- side-by-side compare reading baseline
- Markdown/HTML/JSON outputs
- MCP integration points for OCR, terminology, and document analysis
- skill framework baseline
- multi-model routing baseline

Anything outside this list must be treated as future work unless explicitly approved.

## 3. Non-goals for Phase 1

The following are explicitly out of scope unless re-approved:
- scanned-PDF-first workflow
- high-fidelity page reconstruction in translated output
- full WYSIWYG PDF regeneration
- mobile client support
- cloud service backend
- user account system
- collaborative editing
- translation-memory platform productization
- generalized multi-language matrix beyond Chinese and English
- uncontrolled plugin marketplace

## 4. Repository Boundary

Implementation must follow these repository boundaries:

- `src/qtllm/`
  - infrastructure library only
- `src/apps/`
  - infrastructure/demo/host apps only
- `src/agents/pdf_translator_agent/`
  - business-agent product code

Phase-1 business logic must not be pushed into existing `src/apps/*`.

Only infrastructure abstractions that are clearly generic and reusable may be proposed for `src/qtllm/`, and only after confirming they are not business-specific.

## 5. UI Boundary

The product UI must:
- use Qt Widgets as the main shell
- prioritize non-technical user clarity
- keep advanced configuration hidden by default

The product UI must not:
- default to developer-console style layouts
- expose provider/MCP/raw prompt details in the primary workflow
- require users to understand model vendors or tool schemas

## 6. Skill Boundary

Runtime product skills are business-agent units and must stay in the agent codebase.

Do not treat any of the following as equivalent:
- Codex development skills
- MCP tools
- product runtime skills

Rules:
- skill orchestration belongs to the agent product
- MCP is a backend integration path, not the top-level business workflow
- the main workflow must remain controlled by the product pipeline

## 7. Model Routing Boundary

A single global model configuration is not acceptable for this product.

Phase 1 must preserve:
- per-skill endpoint binding
- per-skill execution type
- fallback route capability
- task-level override capability

Do not hard-code one provider/model path into the translation workflow.

## 8. Pipeline Boundary

The pipeline must remain stage-driven:

```text
extract -> normalize -> detect-language -> chunk -> translate -> qa -> assemble
```

Do not collapse this into one large prompt or one opaque end-to-end agent call.

Reasons:
- poorer traceability
- harder retries
- higher cost volatility
- weaker partial recovery
- worse UX for progress reporting

## 9. Persistence Boundary

Output and runtime state must remain separated.

Allowed next to original PDFs:
- translated `.md`
- translated `.html`
- translation manifest `.json`

Internal scheduler/cache/trace state must stay in the application workspace.

Do not write uncontrolled temporary data into user document folders.

## 10. Batch Execution Boundary

Batch translation must use bounded concurrency and resumable state.

Do not implement:
- unbounded background threads
- in-memory-only queue state
- all-or-nothing final writes

Required behaviors:
- resumable per-document state
- chunk-level incremental persistence
- retryable failures
- visible document-level progress

## 11. Reader Boundary

Phase 1 compare reading must optimize for stability, not perfect PDF fidelity.

Allowed in phase 1:
- structured original-text reading
- translated HTML/Markdown reading
- anchor-linked compare navigation

Defer unless re-approved:
- pixel-level PDF fidelity
- full translated PDF page reproduction
- custom page renderer with large maintenance burden

If page-grade rendering is pursued later, prefer evaluating mature embedded technologies such as `QWebEngineView + pdf.js`.

## 12. OCR Boundary

OCR is a fallback capability in phase 1, not the mainline workflow.

Rules:
- primary input assumption remains text-extractable PDF
- OCR should enter only when extraction fails or is inadequate
- OCR should be routed through the skill/MCP framework, not embedded as ad hoc special logic everywhere

## 13. Language Boundary

Phase 1 language scope is fixed to:
- English source -> Chinese target
- Chinese source -> English target

Automatic language detection is required.

Do not broaden to arbitrary multilingual translation in phase 1.

## 14. First Skill Freeze

The first skill used to validate the framework is:
- `language-detect`

This skill must be implemented before larger skill proliferation.

Validation expectations:
- deterministic input/output contract
- local-rule first pass
- lightweight model fallback
- normalized language-code output

Do not begin by building many loosely defined skills without proving the framework on this first one.

## 15. Observability Boundary

Phase 1 must preserve enough observability to answer:
- what stage a document is in
- which skill executed
- which model/backend was used
- why a step failed
- what artifacts were produced

Reuse existing logging and `toolsinside` where appropriate, but do not make deep observability a user-facing burden in the default UI.

## 16. Change Control

Any proposed change that violates this document should be treated as one of:
- future enhancement
- separate phase proposal
- explicit scope change request

Implementation should not silently drift by accumulating "small exceptions".

When a feature request conflicts with this document, the default action is:
1. stop
2. identify the violated boundary
3. classify it as phase-2 or later unless re-approved
