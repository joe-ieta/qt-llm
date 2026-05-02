# PDF Translator Agent Delivery Checklist

## Scope

This checklist compares the current implementation with the original agreed product scope for the PDF Translator Agent. It is intended to freeze delivery status, identify remaining gaps, and provide a stable work queue for the next iteration without changing runtime behavior.

Reference requirements:

1. A document translation agent based on current disk directory contents, handling PDF documents only.
2. Three modes: fragment translation, single-document translation, and batch background translation.
3. Fragment translation should feel like a chat window with iterative re-translation; document translation should support side-by-side reading and show translation progress; batch translation should support concurrent background processing with per-document progress and compare reading after completion.
4. Source and translated outputs must be stored on disk, with translated files placed next to the source using different names.
5. The translator must be an independent business agent, isolated from current `src/apps`, while reusing the current LLM/MCP base capabilities. It should run on Windows and Linux.

Confirmed follow-up constraints:

- Phase 1 only supports text-extractable PDFs.
- Output may land as `*.md` / `*.html` / `*.json`.
- OCR, terminology, and document parsing should be connected through MCP-oriented skill design.
- UI baseline is Qt Widgets.
- Phase 1 language scope is Chinese-English translation with automatic source language detection.
- Different skills may use different models or backends.

## Delivery Status

### Completed

- Independent business agent project created under `src/agents/pdf_translator_agent/`.
- Existing `src/qtllm` base capabilities reused; `src/apps` remains isolated.
- Fragment translation mode implemented.
- Single-document translation mode implemented.
- Batch background translation mode implemented.
- Skill framework implemented, including skill registration, skill routing, and skill-level backend binding.
- Per-skill model routing implemented for language detection and translation.
- MCP-oriented bindings implemented for PDF extraction fallback and terminology enhancement.
- Disk persistence implemented for translated artifacts and manifests.
- Side-by-side compare reading implemented for extracted original text and translated output.
- Batch queue supports pause, resume, retry, limited concurrency, result opening, source-folder opening, and loading a batch result into compare reading.
- UI parameters persist across restarts through `QSettings`.
- Batch queue state persists across restarts and resumes queued work.
- Single-document and batch workflows now expose stage-level real-time progress updates.

### Partially Completed

- PDF reading experience:
  Current implementation provides text/HTML compare reading, not a page-grade PDF reader.
  This satisfies the core compare-reading flow, but does not yet satisfy a true PDF-reader-grade original-document experience.

- Chat-style fragment translation:
  Current implementation supports iterative re-translation, history, and feedback-aware revision.
  It is a lightweight conversation workflow rather than a full message-thread conversation system with editable turns, branch re-asks, or streamed assistant turns.

- MCP OCR capability:
  The architecture supports MCP-based parsing fallback and skill-based backend routing.
  A dedicated OCR skill is not yet separately implemented and validated as a first-class runtime capability.

- Cross-platform delivery:
  The codebase is designed for Windows and Linux through Qt Widgets and filesystem-safe behavior.
  The current build has been verified on Windows only; Linux build and runtime verification are still pending.

- JSON translation output:
  The system persists manifest JSON and translated Markdown/HTML.
  A dedicated translated-content JSON output file is not yet generated as a first-class export artifact.

## Remaining Functional Gaps Against Original Intent

### High Priority

- Add a dedicated PDF page reader strategy for the original document side.
  Recommended direction: evaluate `Qt PDF` versus `QWebEngineView + pdf.js`, then implement page navigation, zoom, and page-position anchoring.

- Add Linux build verification and runtime smoke testing.
  The requirement is cross-platform operation, so delivery should not be considered fully closed before Linux validation.

- Promote OCR to an explicit skill rather than only an MCP-capable fallback pattern.
  Even if text-extractable PDFs remain the default, the architecture requirement already assumes OCR as a distinct skill-capability class.

### Medium Priority

- Add a dedicated translated JSON export.
  Recommended artifact: a structured file containing source path, languages, chunk mapping, translated text, and output paths.

- Strengthen fragment conversation semantics.
  Add per-turn timestamps, turn IDs, resend/revise actions, and optional preserved context across multiple turns.

- Add batch filtering and operational controls.
  Suggested additions: failed-only filter, completed-only filter, sorting, and context menu actions.

### Lower Priority

- Add richer progress telemetry.
  Current workflow progress is stage-level. Later iterations can expose chunk-level progress, estimated remaining time, and throughput metrics.

- Add configuration presets for non-technical users.
  Example presets: `Fast Local`, `High Quality Cloud`, `With Terminology`, `With MCP Parsing`.

- Add settings validation summaries.
  Current endpoint tests are action-driven. Later iterations can provide a consolidated readiness summary panel.

## Optimization Opportunities

### Product Experience

- Move from debug-oriented text panes toward user-facing result panels by default, with debug views collapsible.
- Add explicit onboarding hints when no model or MCP binding is configured.
- Add clearer outcome labels for generated files, including direct open actions in single-document mode.

### Architecture

- Introduce a dedicated `SettingsRepository` instead of persisting all runtime UI state directly in `MainWindow`.
- Separate long-running workflow execution from UI-triggered synchronous calls in single-document mode.
- Add a lightweight workflow event model shared by fragment, document, and batch flows.

### Reliability

- Add queue persistence versioning to protect future format evolution.
- Add result recovery from manifest files even if `QSettings` state is lost.
- Add defensive validation around missing output files when restoring batch tasks.

### Testing

- Add unit tests for `DocumentTranslationTask` serialization and stage transitions.
- Add workflow tests for queue restore behavior and stage-progress callbacks.
- Add integration tests for skill routing, especially mixed LLM/MCP configurations.

## Recommended Next Work Queue

1. Implement Linux build and smoke-test validation.
2. Add dedicated OCR skill abstraction and MCP-backed implementation.
3. Implement reader-grade original PDF viewing.
4. Add translated JSON artifact export.
5. Add batch filters, sorting, and context menus.
6. Refactor settings persistence into a dedicated repository.
7. Add automated tests for workflow progress and queue restore.

## Release Assessment

Current status: usable Phase 1 product baseline, not final closure against the full original product ambition.

The main reason it is not yet fully closed is not missing core workflow coverage. The main remaining gaps are:

- page-grade PDF reading,
- explicit OCR skill completion,
- Linux validation,
- stronger export and recovery completeness.
