#include "mainwindow.h"

#include "../domain/documenttranslationtask.h"
#include "../pipeline/batchtranslationqueuecontroller.h"
#include "../pipeline/documentworkflowcontroller.h"
#include "../skills/core/modelrouter.h"
#include "../skills/core/skillregistry.h"
#include "../skills/mcp/mcpgateway.h"
#include "../storage/manifestrepository.h"
#include "../viewer/comparereaderwidget.h"

#include "../../../qtllm/identity/compactid.h"
#include "../../../qtllm/core/llmtypes.h"
#include "../../../qtllm/core/qtllmclient.h"

#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QDesktopServices>
#include <QEventLoop>
#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QSpinBox>
#include <QSplitter>
#include <QTabWidget>
#include <QTableWidget>
#include <QHeaderView>
#include <QSettings>
#include <QTextEdit>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QCloseEvent>

namespace pdftranslator {

namespace {

QString defaultBaseUrlForProvider(const QString &provider)
{
    const QString normalized = provider.trimmed().toLower();
    if (normalized == QStringLiteral("ollama")) {
        return QStringLiteral("http://127.0.0.1:11434/v1");
    }
    if (normalized == QStringLiteral("openai")) {
        return QStringLiteral("https://api.openai.com/v1");
    }
    if (normalized == QStringLiteral("vllm")) {
        return QStringLiteral("http://127.0.0.1:8000/v1");
    }
    return QStringLiteral("http://127.0.0.1:11434/v1");
}

QString compactJson(const QJsonObject &object)
{
    return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Indented));
}

QString htmlEscaped(const QString &text)
{
    QString escaped = text.toHtmlEscaped();
    escaped.replace(QStringLiteral("\n"), QStringLiteral("<br/>"));
    return escaped;
}

QString fragmentRoleTitle(const QString &role)
{
    if (role == QStringLiteral("source")) {
        return QStringLiteral("Source");
    }
    if (role == QStringLiteral("feedback")) {
        return QStringLiteral("Revision Request");
    }
    if (role == QStringLiteral("translation")) {
        return QStringLiteral("Translation");
    }
    return role;
}

QString fragmentBubbleStyle(const QString &role)
{
    if (role == QStringLiteral("translation")) {
        return QStringLiteral("background:#EEF6FF;border:1px solid #BFD6F2;color:#16324F;");
    }
    if (role == QStringLiteral("feedback")) {
        return QStringLiteral("background:#FFF7E8;border:1px solid #E7C98A;color:#5A4214;");
    }
    return QStringLiteral("background:#F4F6F8;border:1px solid #D5DADF;color:#1F2933;");
}

QString fragmentHistoryToHtml(const QJsonArray &history)
{
    QString html = QStringLiteral(
        "<html><body style=\"font-family:'Segoe UI'; background:#FAFBFC; margin:12px;\">");
    for (const QJsonValue &value : history) {
        if (!value.isObject()) {
            continue;
        }
        const QJsonObject object = value.toObject();
        const QString role = object.value(QStringLiteral("role")).toString();
        const QString text = object.value(QStringLiteral("text")).toString();
        if (!text.trimmed().isEmpty()) {
            html += QStringLiteral(
                        "<div style=\"margin-bottom:12px;\">"
                        "<div style=\"font-size:11px;font-weight:600;color:#52606D;margin-bottom:4px;\">%1</div>"
                        "<div style=\"border-radius:10px;padding:10px 12px;%2\">%3</div>"
                        "</div>")
                        .arg(fragmentRoleTitle(role),
                             fragmentBubbleStyle(role),
                             htmlEscaped(text.trimmed()));
        }
    }
    html += QStringLiteral("</body></html>");
    return html;
}

QString runEndpointSmokeTest(const pdftranslator::skills::ModelEndpointConfig &endpoint)
{
    qtllm::QtLLMClient client;
    client.setConfig(endpoint.llmConfig);
    if (!client.setProviderByName(endpoint.llmConfig.providerName)) {
        return QStringLiteral("Provider resolution failed");
    }

    qtllm::LlmRequest request;
    request.model = endpoint.llmConfig.model;
    request.stream = false;
    request.messages.append({QStringLiteral("system"), QStringLiteral("Return exactly OK.")});
    request.messages.append({QStringLiteral("user"), QStringLiteral("Reply with OK only.")});

    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QString responseText;
    QString errorMessage;
    QObject::connect(&client, &qtllm::QtLLMClient::responseReceived, &loop, [&](const qtllm::LlmResponse &response) {
        responseText = response.assistantMessage.content.isEmpty() ? response.text : response.assistantMessage.content;
        errorMessage = response.errorMessage;
        loop.quit();
    });
    QObject::connect(&client, &qtllm::QtLLMClient::errorOccurred, &loop, [&](const QString &message) {
        errorMessage = message;
        loop.quit();
    });
    QObject::connect(&timeout, &QTimer::timeout, &loop, [&]() {
        errorMessage = QStringLiteral("Endpoint test timed out");
        client.cancelCurrentRequest();
        loop.quit();
    });

    timeout.start(endpoint.llmConfig.timeoutMs > 0 ? endpoint.llmConfig.timeoutMs : 15000);
    client.sendRequest(request);
    loop.exec();

    if (!errorMessage.trimmed().isEmpty()) {
        return errorMessage.trimmed();
    }

    return responseText.trimmed().isEmpty()
        ? QStringLiteral("Endpoint returned an empty response")
        : QStringLiteral("OK: %1").arg(responseText.trimmed());
}

} // namespace

MainWindow::MainWindow(const std::shared_ptr<skills::SkillRegistry> &skillRegistry,
                       const std::shared_ptr<skills::ModelRouter> &modelRouter,
                       const std::shared_ptr<skills::mcp::McpGateway> &mcpGateway,
                       QWidget *parent)
    : QWidget(parent)
    , m_skillRegistry(skillRegistry)
    , m_modelRouter(modelRouter)
    , m_mcpGateway(mcpGateway)
    , m_workflowController(std::make_unique<pipeline::DocumentWorkflowController>(skillRegistry, modelRouter))
    , m_batchQueueController(std::make_unique<pipeline::BatchTranslationQueueController>(skillRegistry, modelRouter))
{
    buildUi();
    loadUiState();
    connectPersistenceSignals();
}

void MainWindow::onDetectLanguageClicked()
{
    applyLanguageDetectBinding();
    m_statusLabel->setText(QStringLiteral("Running language-detect..."));

    const std::shared_ptr<skills::ISkill> skill = m_skillRegistry ? m_skillRegistry->skill(QStringLiteral("language-detect")) : nullptr;
    if (!skill) {
        m_statusLabel->setText(QStringLiteral("language-detect skill is not registered"));
        return;
    }

    skills::SkillContext context;
    context.taskId = QStringLiteral("manual-language-detect");
    context.sourceText = m_inputEdit->toPlainText();

    const skills::SkillResult result = skill->execute(context);
    if (!result.success) {
        m_statusLabel->setText(QStringLiteral("Language detection failed"));
        m_resultView->setPlainText(result.errorMessage);
        return;
    }

    QString text = compactJson(result.output);
    if (!result.warnings.isEmpty()) {
        text += QStringLiteral("\nWarnings:\n");
        for (const skills::SkillWarning &warning : result.warnings) {
            text += QStringLiteral("- [%1] %2\n").arg(warning.code, warning.message);
        }
    }

    m_resultView->setPlainText(text.trimmed());
    m_statusLabel->setText(QStringLiteral("Language detection completed"));
}

void MainWindow::onTranslateFragmentClicked()
{
    const QString sourceText = m_inputEdit->toPlainText().trimmed();
    if (sourceText.isEmpty()) {
        m_statusLabel->setText(QStringLiteral("Enter source text first"));
        return;
    }

    applyLanguageDetectBinding();
    applyTranslateBinding();

    const std::shared_ptr<skills::ISkill> detectSkill = m_skillRegistry ? m_skillRegistry->skill(QStringLiteral("language-detect")) : nullptr;
    const std::shared_ptr<skills::ISkill> translateSkill = m_skillRegistry ? m_skillRegistry->skill(QStringLiteral("chunk-translate")) : nullptr;
    if (!translateSkill) {
        m_statusLabel->setText(QStringLiteral("chunk-translate skill is not registered"));
        return;
    }

    QString sourceLanguage = QStringLiteral("en");
    QString targetLanguage = QStringLiteral("zh-CN");
    QJsonObject detectSummary;

    if (detectSkill) {
        skills::SkillContext detectContext;
        detectContext.taskId = QStringLiteral("manual-fragment-detect");
        detectContext.sourceText = sourceText.left(4000);
        const skills::SkillResult detectResult = detectSkill->execute(detectContext);
        if (detectResult.success) {
            sourceLanguage = detectResult.output.value(QStringLiteral("sourceLanguage")).toString(sourceLanguage);
            targetLanguage = detectResult.output.value(QStringLiteral("targetLanguage")).toString(targetLanguage);
            detectSummary = detectResult.output;
        }
    }

    const QString targetSelection = m_fragmentTargetLanguageCombo->currentData().toString();
    if (!targetSelection.isEmpty()) {
        targetLanguage = targetSelection;
    }

    skills::SkillContext translateContext;
    translateContext.taskId = QStringLiteral("manual-fragment-translate");
    translateContext.sourceText = sourceText;
    translateContext.sourceLanguageHint = sourceLanguage;
    translateContext.targetLanguageHint = targetLanguage;
    translateContext.extra.insert(QStringLiteral("translationInstructions"), m_fragmentInstructionsEdit->text().trimmed());
    translateContext.extra.insert(QStringLiteral("previousTranslation"), m_lastFragmentTranslation);
    translateContext.extra.insert(QStringLiteral("userFeedback"), m_fragmentInstructionsEdit->text().trimmed());

    m_statusLabel->setText(QStringLiteral("Running fragment translation..."));
    const skills::SkillResult translateResult = translateSkill->execute(translateContext);
    if (!translateResult.success) {
        m_statusLabel->setText(QStringLiteral("Fragment translation failed"));
        m_resultView->setPlainText(translateResult.errorMessage);
        return;
    }

    QString resultText = translateResult.output.value(QStringLiteral("translatedText")).toString().trimmed();
    if (!detectSummary.isEmpty()) {
        resultText = QStringLiteral("[Source: %1 | Target: %2]\n\n%3")
                         .arg(sourceLanguage, targetLanguage, resultText);
    }

    m_resultView->setPlainText(resultText);
    m_lastFragmentTranslation = translateResult.output.value(QStringLiteral("translatedText")).toString().trimmed();
    m_lastFragmentSourceLanguage = sourceLanguage;
    m_lastFragmentTargetLanguage = targetLanguage;
    appendFragmentHistoryEntry(QStringLiteral("source"), sourceText);
    if (!m_fragmentInstructionsEdit->text().trimmed().isEmpty()) {
        appendFragmentHistoryEntry(QStringLiteral("feedback"), m_fragmentInstructionsEdit->text().trimmed());
    }
    appendFragmentHistoryEntry(QStringLiteral("translation"), m_lastFragmentTranslation);
    m_statusLabel->setText(QStringLiteral("Fragment translation completed"));
}

void MainWindow::onClearFragmentHistoryClicked()
{
    m_fragmentHistory = QJsonArray();
    m_lastFragmentTranslation.clear();
    m_lastFragmentSourceLanguage.clear();
    m_lastFragmentTargetLanguage.clear();
    refreshFragmentHistoryView();
    saveUiState();
}

void MainWindow::onBrowsePdfClicked()
{
    const QString filePath = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("Select PDF"),
        QString(),
        QStringLiteral("PDF Files (*.pdf)"));
    if (!filePath.isEmpty()) {
        m_pdfPathEdit->setText(filePath);
        saveUiState();
    }
}

void MainWindow::onExtractPdfClicked()
{
    const QString pdfPath = m_pdfPathEdit->text().trimmed();
    if (pdfPath.isEmpty()) {
        m_statusLabel->setText(QStringLiteral("Select a PDF file first"));
        return;
    }

    applyPdfExtractBinding();
    const std::shared_ptr<skills::ISkill> extractSkill = m_skillRegistry ? m_skillRegistry->skill(QStringLiteral("pdf-text-extract")) : nullptr;
    if (!extractSkill) {
        m_statusLabel->setText(QStringLiteral("pdf-text-extract skill is not registered"));
        return;
    }

    domain::DocumentTranslationTask task;
    task.taskId = qtllm::identity::generateId(qtllm::identity::IdKind::Task);
    task.sourcePdfPath = pdfPath;
    task.manifestPath = defaultManifestPathForPdf(pdfPath);
    task.transitionTo(domain::DocumentTaskStage::Extracting);
    task.progress = 10;

    skills::SkillContext extractContext;
    extractContext.taskId = task.taskId;
    extractContext.documentPath = pdfPath;

    m_statusLabel->setText(QStringLiteral("Running pdf-text-extract..."));
    const skills::SkillResult extraction = extractSkill->execute(extractContext);
    if (!extraction.success) {
        task.transitionTo(domain::DocumentTaskStage::Failed, extraction.errorMessage);
        task.progress = 0;

        storage::ManifestRepository repository;
        QString saveError;
        repository.save(task, &saveError);
        m_pdfExtractView->setPlainText(extraction.errorMessage);
        m_manifestView->setPlainText(compactJson(task.toJson()));
        m_statusLabel->setText(QStringLiteral("PDF extraction failed"));
        return;
    }

    const QString extractedText = extraction.output.value(QStringLiteral("text")).toString();
    task.transitionTo(domain::DocumentTaskStage::DetectingLanguage);
    task.progress = 40;
    task.extractedTextPath = QFileInfo(task.manifestPath).absolutePath()
        + QDir::separator()
        + QFileInfo(pdfPath).completeBaseName()
        + QStringLiteral(".extracted.txt");

    QFile extractedFile(task.extractedTextPath);
    if (extractedFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        extractedFile.write(extractedText.toUtf8());
        extractedFile.close();
    }

    const std::shared_ptr<skills::ISkill> detectSkill = m_skillRegistry ? m_skillRegistry->skill(QStringLiteral("language-detect")) : nullptr;
    if (detectSkill) {
        applyLanguageDetectBinding();
        skills::SkillContext detectContext;
        detectContext.taskId = task.taskId;
        detectContext.documentPath = pdfPath;
        detectContext.sourceText = extractedText.left(4000);
        const skills::SkillResult detection = detectSkill->execute(detectContext);
        if (detection.success) {
            task.sourceLanguage = detection.output.value(QStringLiteral("sourceLanguage")).toString();
            task.targetLanguage = detection.output.value(QStringLiteral("targetLanguage")).toString();
            task.progress = 60;
        } else {
            task.lastError = detection.errorMessage;
        }
    }

    task.transitionTo(domain::DocumentTaskStage::Queued);
    task.progress = 65;

    storage::ManifestRepository repository;
    QString saveError;
    repository.save(task, &saveError);

    m_pdfExtractView->setPlainText(extractedText);
    m_manifestView->setPlainText(compactJson(task.toJson()));
    m_statusLabel->setText(QStringLiteral("PDF extraction completed; task baseline persisted"));
}

void MainWindow::onTranslateDocumentClicked()
{
    const QString pdfPath = m_pdfPathEdit->text().trimmed();
    if (pdfPath.isEmpty()) {
        m_statusLabel->setText(QStringLiteral("Select a PDF file first"));
        return;
    }

    applyLanguageDetectBinding();
    applyTranslateBinding();
    applyPdfExtractBinding();
    applyTermBaseBinding();

    if (!m_workflowController) {
        m_statusLabel->setText(QStringLiteral("Workflow controller is not available"));
        return;
    }

    m_statusLabel->setText(QStringLiteral("Running document translation workflow..."));
    m_documentStageLabel->setText(QStringLiteral("queued"));
    m_documentProgressBar->setRange(0, 100);
    m_documentProgressBar->setValue(0);
    const pipeline::DocumentWorkflowRunResult runResult = m_workflowController->runSingleDocumentTranslation(
        pdfPath,
        [this](const domain::DocumentTranslationTask &task) {
            m_documentStageLabel->setText(domain::documentTaskStageToString(task.stage));
            m_documentProgressBar->setValue(task.progress);
            QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        });
    m_documentProgressBar->setValue(runResult.task.progress);
    m_pdfExtractView->setPlainText(runResult.extractedText);
    m_translatedView->setPlainText(runResult.translatedText.isEmpty() ? runResult.errorMessage : runResult.translatedText);
    m_manifestView->setPlainText(compactJson(runResult.task.toJson()));
    m_compareReader->setOriginalText(runResult.extractedText);
    m_compareReader->setTranslatedText(runResult.translatedText.isEmpty() ? runResult.errorMessage : runResult.translatedText);
    m_compareReader->setSourceLabel(QStringLiteral("Original (%1)").arg(runResult.task.sourceLanguage.isEmpty() ? QStringLiteral("unknown") : runResult.task.sourceLanguage));
    m_compareReader->setTargetLabel(QStringLiteral("Translated (%1)").arg(runResult.task.targetLanguage.isEmpty() ? QStringLiteral("unknown") : runResult.task.targetLanguage));
    m_compareReader->setOutputPaths(runResult.task.translatedMarkdownPath, runResult.task.translatedHtmlPath);

    QFile htmlFile(runResult.task.translatedHtmlPath);
    if (htmlFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_compareReader->setTranslatedHtml(QString::fromUtf8(htmlFile.readAll()));
        htmlFile.close();
    }

    if (runResult.task.stage == domain::DocumentTaskStage::Completed) {
        m_documentStageLabel->setText(QStringLiteral("Completed"));
        m_statusLabel->setText(QStringLiteral("Document translation workflow completed"));
    } else {
        m_documentStageLabel->setText(QStringLiteral("Failed"));
        m_statusLabel->setText(QStringLiteral("Document translation workflow failed"));
    }
}

void MainWindow::onAddBatchDocumentsClicked()
{
    const QStringList filePaths = QFileDialog::getOpenFileNames(
        this,
        QStringLiteral("Select PDF Documents"),
        QString(),
        QStringLiteral("PDF Files (*.pdf)"));
    if (filePaths.isEmpty()) {
        return;
    }

    applyLanguageDetectBinding();
    applyTranslateBinding();
    applyPdfExtractBinding();
    applyTermBaseBinding();

    if (!m_batchQueueController) {
        m_statusLabel->setText(QStringLiteral("Batch queue controller is not available"));
        return;
    }

    m_batchQueueController->enqueueDocuments(filePaths);
    refreshBatchTable();
    m_statusLabel->setText(QStringLiteral("Batch translation queue updated"));
}

void MainWindow::onPauseResumeBatchClicked()
{
    if (!m_batchQueueController) {
        return;
    }

    m_batchQueueController->setPaused(!m_batchQueueController->isPaused());
    refreshBatchTable();
    saveUiState();
}

void MainWindow::onRetrySelectedBatchClicked()
{
    if (!m_batchQueueController || !m_batchTable || m_batchTable->currentRow() < 0) {
        return;
    }

    const int row = m_batchTable->currentRow();
    const QVector<pipeline::BatchQueueEntry> entries = m_batchQueueController->entries();
    if (row < 0 || row >= entries.size()) {
        return;
    }

    if (m_batchQueueController->retryEntry(entries.at(row).queueId)) {
        m_statusLabel->setText(QStringLiteral("Selected batch task queued for retry"));
    }
}

void MainWindow::onTestLanguageDetectEndpointClicked()
{
    applyLanguageDetectBinding();
    if (!m_modelRouter) {
        m_statusLabel->setText(QStringLiteral("Model router is not available"));
        return;
    }

    const auto endpoint = m_modelRouter->endpoint(QStringLiteral("language-detect-default"));
    m_statusLabel->setText(endpoint.has_value()
                               ? QStringLiteral("Testing language-detect endpoint...")
                               : QStringLiteral("Language-detect endpoint is not configured"));
    if (!endpoint.has_value()) {
        return;
    }

    m_statusLabel->setText(QStringLiteral("Language-detect endpoint test: %1").arg(runEndpointSmokeTest(*endpoint)));
}

void MainWindow::onTestTranslateEndpointClicked()
{
    applyTranslateBinding();
    if (!m_modelRouter) {
        m_statusLabel->setText(QStringLiteral("Model router is not available"));
        return;
    }

    const auto endpoint = m_modelRouter->endpoint(QStringLiteral("chunk-translate-default"));
    m_statusLabel->setText(endpoint.has_value()
                               ? QStringLiteral("Testing translate endpoint...")
                               : QStringLiteral("Translate endpoint is not configured"));
    if (!endpoint.has_value()) {
        return;
    }

    m_statusLabel->setText(QStringLiteral("Translate endpoint test: %1").arg(runEndpointSmokeTest(*endpoint)));
}

void MainWindow::onTestExtractMcpClicked()
{
    applyPdfExtractBinding();
    if (!m_mcpGateway) {
        m_statusLabel->setText(QStringLiteral("MCP gateway is not available"));
        return;
    }

    const QString serverId = m_extractMcpServerIdEdit->text().trimmed();
    const QString toolName = m_extractMcpToolNameEdit->text().trimmed();
    if (serverId.isEmpty() || toolName.isEmpty()) {
        m_statusLabel->setText(QStringLiteral("Enter PDF extract MCP server and tool first"));
        return;
    }

    QString errorMessage;
    m_mcpGateway->loadServers(&errorMessage);
    const auto server = m_mcpGateway->findServer(serverId);
    m_statusLabel->setText(server.has_value()
                               ? QStringLiteral("PDF extract MCP binding looks available: %1 / %2").arg(serverId, toolName)
                               : QStringLiteral("PDF extract MCP test failed: %1").arg(errorMessage.isEmpty()
                                                                                           ? QStringLiteral("server not found")
                                                                                           : errorMessage));
}

void MainWindow::onTestTermMcpClicked()
{
    applyTermBaseBinding();
    if (!m_mcpGateway) {
        m_statusLabel->setText(QStringLiteral("MCP gateway is not available"));
        return;
    }

    const QString serverId = m_termMcpServerIdEdit->text().trimmed();
    const QString toolName = m_termMcpToolNameEdit->text().trimmed();
    if (serverId.isEmpty() || toolName.isEmpty()) {
        m_statusLabel->setText(QStringLiteral("Enter term-base MCP server and tool first"));
        return;
    }

    QString errorMessage;
    m_mcpGateway->loadServers(&errorMessage);
    const auto server = m_mcpGateway->findServer(serverId);
    m_statusLabel->setText(server.has_value()
                               ? QStringLiteral("Term-base MCP binding looks available: %1 / %2").arg(serverId, toolName)
                               : QStringLiteral("Term-base MCP test failed: %1").arg(errorMessage.isEmpty()
                                                                                         ? QStringLiteral("server not found")
                                                                                         : errorMessage));
}

void MainWindow::onOpenSelectedBatchResultClicked()
{
    if (!m_batchQueueController) {
        return;
    }

    const int row = selectedBatchRow();
    const QVector<pipeline::BatchQueueEntry> entries = m_batchQueueController->entries();
    if (row < 0 || row >= entries.size()) {
        m_statusLabel->setText(QStringLiteral("Select a batch task first"));
        return;
    }

    const QString outputPath = entries.at(row).task.translatedHtmlPath.isEmpty()
        ? entries.at(row).task.translatedMarkdownPath
        : entries.at(row).task.translatedHtmlPath;
    if (outputPath.isEmpty() || !QFileInfo::exists(outputPath)) {
        m_statusLabel->setText(QStringLiteral("Selected batch task has no generated result yet"));
        return;
    }

    QDesktopServices::openUrl(QUrl::fromLocalFile(outputPath));
    m_statusLabel->setText(QStringLiteral("Opened batch result"));
}

void MainWindow::onOpenSelectedBatchFolderClicked()
{
    if (!m_batchQueueController) {
        return;
    }

    const int row = selectedBatchRow();
    const QVector<pipeline::BatchQueueEntry> entries = m_batchQueueController->entries();
    if (row < 0 || row >= entries.size()) {
        m_statusLabel->setText(QStringLiteral("Select a batch task first"));
        return;
    }

    const QFileInfo info(entries.at(row).pdfPath);
    QDesktopServices::openUrl(QUrl::fromLocalFile(info.absolutePath()));
    m_statusLabel->setText(QStringLiteral("Opened source folder"));
}

void MainWindow::onLoadSelectedBatchCompareClicked()
{
    if (!m_batchQueueController) {
        return;
    }

    const int row = selectedBatchRow();
    const QVector<pipeline::BatchQueueEntry> entries = m_batchQueueController->entries();
    if (row < 0 || row >= entries.size()) {
        m_statusLabel->setText(QStringLiteral("Select a batch task first"));
        return;
    }

    const pipeline::BatchQueueEntry &entry = entries.at(row);
    m_pdfPathEdit->setText(entry.pdfPath);
    m_pdfExtractView->setPlainText(entry.extractedText);
    m_translatedView->setPlainText(entry.translatedText);
    m_manifestView->setPlainText(compactJson(entry.task.toJson()));
    m_compareReader->setOriginalText(entry.extractedText);
    m_compareReader->setTranslatedText(entry.translatedText.isEmpty() ? entry.task.lastError : entry.translatedText);
    m_compareReader->setSourceLabel(QStringLiteral("Original (%1)").arg(entry.task.sourceLanguage.isEmpty() ? QStringLiteral("unknown") : entry.task.sourceLanguage));
    m_compareReader->setTargetLabel(QStringLiteral("Translated (%1)").arg(entry.task.targetLanguage.isEmpty() ? QStringLiteral("unknown") : entry.task.targetLanguage));
    m_compareReader->setOutputPaths(entry.task.translatedMarkdownPath, entry.task.translatedHtmlPath);
    QFile htmlFile(entry.task.translatedHtmlPath);
    if (htmlFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_compareReader->setTranslatedHtml(QString::fromUtf8(htmlFile.readAll()));
        htmlFile.close();
    }
    m_tabs->setCurrentIndex(1);
    m_statusLabel->setText(QStringLiteral("Loaded selected batch task into compare reading"));
}

void MainWindow::refreshBatchTable()
{
    if (!m_batchQueueController || !m_batchTable) {
        return;
    }

    const QVector<pipeline::BatchQueueEntry> entries = m_batchQueueController->entries();
    m_batchTable->setRowCount(entries.size());

    for (int row = 0; row < entries.size(); ++row) {
        const pipeline::BatchQueueEntry &entry = entries.at(row);
        const QFileInfo info(entry.pdfPath);
        m_batchTable->setItem(row, 0, new QTableWidgetItem(info.fileName()));
        m_batchTable->setItem(row, 1, new QTableWidgetItem(domain::documentTaskStageToString(entry.task.stage)));
        m_batchTable->setItem(row, 2, new QTableWidgetItem(QString::number(entry.task.progress) + QStringLiteral("%")));
        m_batchTable->setItem(row, 3, new QTableWidgetItem(entry.task.sourceLanguage));
        m_batchTable->setItem(row, 4, new QTableWidgetItem(entry.task.targetLanguage));
        m_batchTable->setItem(row, 5, new QTableWidgetItem(entry.task.lastError));
        m_batchTable->setItem(row, 6, new QTableWidgetItem(entry.task.translatedMarkdownPath));
    }

    m_batchTable->resizeRowsToContents();
    m_batchStatusLabel->setText(QStringLiteral("Active %1 / %2, %3")
                                    .arg(m_batchQueueController->activeJobCount())
                                    .arg(m_batchQueueController->maxConcurrentJobs())
                                    .arg(m_batchQueueController->isPaused() ? QStringLiteral("Paused") : QStringLiteral("Running")));
    m_pauseResumeBatchButton->setText(m_batchQueueController->isPaused()
                                          ? QStringLiteral("Resume Queue")
                                          : QStringLiteral("Pause Queue"));
}

void MainWindow::buildUi()
{
    m_titleLabel = new QLabel(QStringLiteral("PDF Translator Agent"), this);
    m_titleLabel->setStyleSheet(QStringLiteral("font-size: 22px; font-weight: 600;"));

    m_statusLabel = new QLabel(QStringLiteral("Phase-1 skeleton: skill framework + language-detect validation"), this);

    m_endpointGroup = new QGroupBox(QStringLiteral("Language-Detect Skill Endpoint"), this);
    m_providerCombo = new QComboBox(this);
    m_providerCombo->addItems(QStringList({
        QStringLiteral("ollama"),
        QStringLiteral("openai-compatible"),
        QStringLiteral("vllm"),
        QStringLiteral("openai")
    }));

    m_baseUrlEdit = new QLineEdit(defaultBaseUrlForProvider(m_providerCombo->currentText()), this);
    m_apiKeyEdit = new QLineEdit(this);
    m_apiKeyEdit->setEchoMode(QLineEdit::Password);
    m_apiKeyEdit->setPlaceholderText(QStringLiteral("Optional for local/openai-compatible endpoints"));
    m_modelEdit = new QLineEdit(this);
    m_modelEdit->setPlaceholderText(QStringLiteral("Bind a lightweight model for language-detect fallback"));

    auto *endpointForm = new QFormLayout();
    endpointForm->addRow(QStringLiteral("Provider"), m_providerCombo);
    endpointForm->addRow(QStringLiteral("Base URL"), m_baseUrlEdit);
    endpointForm->addRow(QStringLiteral("API Key"), m_apiKeyEdit);
    endpointForm->addRow(QStringLiteral("Model"), m_modelEdit);
    m_testLanguageDetectEndpointButton = new QPushButton(QStringLiteral("Test Language-Detect Endpoint"), this);
    connect(m_testLanguageDetectEndpointButton, &QPushButton::clicked, this, &MainWindow::onTestLanguageDetectEndpointClicked);
    endpointForm->addRow(QString(), m_testLanguageDetectEndpointButton);
    m_endpointGroup->setLayout(endpointForm);

    connect(m_providerCombo, &QComboBox::currentTextChanged, this, [this](const QString &provider) {
        m_baseUrlEdit->setText(defaultBaseUrlForProvider(provider));
    });

    m_inputEdit = new QPlainTextEdit(this);
    m_inputEdit->setPlaceholderText(QStringLiteral("Paste English or Chinese text here to validate the first runtime skill."));
    m_fragmentTargetLanguageCombo = new QComboBox(this);
    m_fragmentTargetLanguageCombo->addItem(QStringLiteral("Auto (opposite language)"), QString());
    m_fragmentTargetLanguageCombo->addItem(QStringLiteral("English"), QStringLiteral("en"));
    m_fragmentTargetLanguageCombo->addItem(QStringLiteral("Chinese (Simplified)"), QStringLiteral("zh-CN"));
    m_fragmentInstructionsEdit = new QLineEdit(this);
    m_fragmentInstructionsEdit->setPlaceholderText(QStringLiteral("Optional refinement, for example: keep technical terms in English"));

    m_detectButton = new QPushButton(QStringLiteral("Run language-detect"), this);
    connect(m_detectButton, &QPushButton::clicked, this, &MainWindow::onDetectLanguageClicked);
    m_translateFragmentButton = new QPushButton(QStringLiteral("Translate Fragment"), this);
    connect(m_translateFragmentButton, &QPushButton::clicked, this, &MainWindow::onTranslateFragmentClicked);

    m_resultView = new QTextEdit(this);
    m_resultView->setReadOnly(true);
    m_fragmentHistoryView = new QTextEdit(this);
    m_fragmentHistoryView->setReadOnly(true);
    m_fragmentHistoryView->setAcceptRichText(true);
    m_fragmentHistoryView->setPlaceholderText(QStringLiteral("Fragment translation history will appear here."));
    m_clearFragmentHistoryButton = new QPushButton(QStringLiteral("Clear History"), this);
    connect(m_clearFragmentHistoryButton, &QPushButton::clicked, this, &MainWindow::onClearFragmentHistoryClicked);

    auto *fragmentButtonRow = new QHBoxLayout();
    fragmentButtonRow->addWidget(new QLabel(QStringLiteral("Target"), this));
    fragmentButtonRow->addWidget(m_fragmentTargetLanguageCombo);
    fragmentButtonRow->addStretch(1);
    fragmentButtonRow->addWidget(m_detectButton);
    fragmentButtonRow->addWidget(m_translateFragmentButton);
    fragmentButtonRow->addWidget(m_clearFragmentHistoryButton);

    auto *fragmentTab = new QWidget(this);
    auto *fragmentLayout = new QVBoxLayout(fragmentTab);
    fragmentLayout->addWidget(new QLabel(QStringLiteral("Fragment Translation"), this));
    auto *fragmentHintLabel = new QLabel(QStringLiteral("Model and MCP settings are managed in the Settings tab."), this);
    fragmentHintLabel->setStyleSheet(QStringLiteral("color:#52606D;"));
    fragmentLayout->addWidget(fragmentHintLabel);
    fragmentLayout->addWidget(new QLabel(QStringLiteral("Validation Input"), this));
    fragmentLayout->addWidget(m_inputEdit, 1);
    fragmentLayout->addWidget(new QLabel(QStringLiteral("Translation Refinement"), this));
    fragmentLayout->addWidget(m_fragmentInstructionsEdit);
    fragmentLayout->addLayout(fragmentButtonRow);
    fragmentLayout->addWidget(new QLabel(QStringLiteral("Skill Result"), this));
    auto *fragmentSplitter = new QSplitter(Qt::Vertical, this);
    fragmentSplitter->addWidget(m_resultView);
    fragmentSplitter->addWidget(m_fragmentHistoryView);
    fragmentSplitter->setStretchFactor(0, 1);
    fragmentSplitter->setStretchFactor(1, 1);
    fragmentLayout->addWidget(fragmentSplitter, 1);
    fragmentTab->setLayout(fragmentLayout);

    m_pdfPathEdit = new QLineEdit(this);
    m_pdfPathEdit->setPlaceholderText(QStringLiteral("Select a text-extractable PDF document"));
    m_browsePdfButton = new QPushButton(QStringLiteral("Browse"), this);
    connect(m_browsePdfButton, &QPushButton::clicked, this, &MainWindow::onBrowsePdfClicked);
    m_extractPdfButton = new QPushButton(QStringLiteral("Extract + Detect"), this);
    connect(m_extractPdfButton, &QPushButton::clicked, this, &MainWindow::onExtractPdfClicked);
    m_translateDocumentButton = new QPushButton(QStringLiteral("Translate Document"), this);
    connect(m_translateDocumentButton, &QPushButton::clicked, this, &MainWindow::onTranslateDocumentClicked);
    m_documentStageLabel = new QLabel(QStringLiteral("Idle"), this);
    m_documentProgressBar = new QProgressBar(this);
    m_documentProgressBar->setRange(0, 100);
    m_documentProgressBar->setValue(0);

    auto *pdfPickerRow = new QHBoxLayout();
    pdfPickerRow->addWidget(m_pdfPathEdit, 1);
    pdfPickerRow->addWidget(m_browsePdfButton);
    pdfPickerRow->addWidget(m_extractPdfButton);
    pdfPickerRow->addWidget(m_translateDocumentButton);

    m_translateEndpointGroup = new QGroupBox(QStringLiteral("Chunk-Translate Skill Endpoint"), this);
    m_translateProviderCombo = new QComboBox(this);
    m_translateProviderCombo->addItems(QStringList({
        QStringLiteral("ollama"),
        QStringLiteral("openai-compatible"),
        QStringLiteral("vllm"),
        QStringLiteral("openai")
    }));
    m_translateBaseUrlEdit = new QLineEdit(defaultBaseUrlForProvider(m_translateProviderCombo->currentText()), this);
    m_translateApiKeyEdit = new QLineEdit(this);
    m_translateApiKeyEdit->setEchoMode(QLineEdit::Password);
    m_translateApiKeyEdit->setPlaceholderText(QStringLiteral("Optional for local/openai-compatible endpoints"));
    m_translateModelEdit = new QLineEdit(this);
    m_translateModelEdit->setPlaceholderText(QStringLiteral("Bind a translation model for chunk-translate"));

    auto *translateForm = new QFormLayout();
    translateForm->addRow(QStringLiteral("Provider"), m_translateProviderCombo);
    translateForm->addRow(QStringLiteral("Base URL"), m_translateBaseUrlEdit);
    translateForm->addRow(QStringLiteral("API Key"), m_translateApiKeyEdit);
    translateForm->addRow(QStringLiteral("Model"), m_translateModelEdit);
    m_testTranslateEndpointButton = new QPushButton(QStringLiteral("Test Translate Endpoint"), this);
    connect(m_testTranslateEndpointButton, &QPushButton::clicked, this, &MainWindow::onTestTranslateEndpointClicked);
    translateForm->addRow(QString(), m_testTranslateEndpointButton);
    m_translateEndpointGroup->setLayout(translateForm);

    connect(m_translateProviderCombo, &QComboBox::currentTextChanged, this, [this](const QString &provider) {
        m_translateBaseUrlEdit->setText(defaultBaseUrlForProvider(provider));
    });

    m_extractMcpGroup = new QGroupBox(QStringLiteral("PDF Extract MCP Fallback"), this);
    m_extractMcpServerIdEdit = new QLineEdit(this);
    m_extractMcpServerIdEdit->setPlaceholderText(QStringLiteral("Optional MCP serverId for document parsing"));
    m_extractMcpToolNameEdit = new QLineEdit(this);
    m_extractMcpToolNameEdit->setPlaceholderText(QStringLiteral("Optional MCP tool name, for example extract_pdf_text"));

    auto *extractMcpForm = new QFormLayout();
    extractMcpForm->addRow(QStringLiteral("Server ID"), m_extractMcpServerIdEdit);
    extractMcpForm->addRow(QStringLiteral("Tool Name"), m_extractMcpToolNameEdit);
    m_testExtractMcpButton = new QPushButton(QStringLiteral("Test PDF Extract MCP Binding"), this);
    connect(m_testExtractMcpButton, &QPushButton::clicked, this, &MainWindow::onTestExtractMcpClicked);
    extractMcpForm->addRow(QString(), m_testExtractMcpButton);
    m_extractMcpGroup->setLayout(extractMcpForm);

    m_termMcpGroup = new QGroupBox(QStringLiteral("Term Base MCP Enhancement"), this);
    m_termMcpServerIdEdit = new QLineEdit(this);
    m_termMcpServerIdEdit->setPlaceholderText(QStringLiteral("Optional MCP serverId for terminology lookup"));
    m_termMcpToolNameEdit = new QLineEdit(this);
    m_termMcpToolNameEdit->setPlaceholderText(QStringLiteral("Optional MCP tool name, for example resolve_terms"));

    auto *termMcpForm = new QFormLayout();
    termMcpForm->addRow(QStringLiteral("Server ID"), m_termMcpServerIdEdit);
    termMcpForm->addRow(QStringLiteral("Tool Name"), m_termMcpToolNameEdit);
    m_testTermMcpButton = new QPushButton(QStringLiteral("Test Term-Base MCP Binding"), this);
    connect(m_testTermMcpButton, &QPushButton::clicked, this, &MainWindow::onTestTermMcpClicked);
    termMcpForm->addRow(QString(), m_testTermMcpButton);
    m_termMcpGroup->setLayout(termMcpForm);

    m_pdfExtractView = new QTextEdit(this);
    m_pdfExtractView->setReadOnly(true);
    m_pdfExtractView->setPlaceholderText(QStringLiteral("Extracted original text preview"));

    m_translatedView = new QTextEdit(this);
    m_translatedView->setReadOnly(true);
    m_translatedView->setPlaceholderText(QStringLiteral("Translated text preview"));

    m_manifestView = new QTextEdit(this);
    m_manifestView->setReadOnly(true);
    m_manifestView->setPlaceholderText(QStringLiteral("Document task manifest preview"));

    m_compareReader = new viewer::CompareReaderWidget(this);

    auto *documentTab = new QWidget(this);
    auto *documentLayout = new QVBoxLayout(documentTab);
    documentLayout->addWidget(new QLabel(QStringLiteral("Single Document Translation Baseline"), this));
    documentLayout->addLayout(pdfPickerRow);
    auto *documentStatusRow = new QHBoxLayout();
    documentStatusRow->addWidget(new QLabel(QStringLiteral("Stage"), this));
    documentStatusRow->addWidget(m_documentStageLabel);
    documentStatusRow->addSpacing(12);
    documentStatusRow->addWidget(new QLabel(QStringLiteral("Progress"), this));
    documentStatusRow->addWidget(m_documentProgressBar, 1);
    documentLayout->addLayout(documentStatusRow);
    auto *documentHintLabel = new QLabel(QStringLiteral("Translation models and MCP tool bindings are configured in the Settings tab."), this);
    documentHintLabel->setStyleSheet(QStringLiteral("color:#52606D;"));
    documentLayout->addWidget(documentHintLabel);
    documentLayout->addWidget(new QLabel(QStringLiteral("Compare Reading"), this));
    documentLayout->addWidget(m_compareReader, 2);
    documentLayout->addWidget(new QLabel(QStringLiteral("Workflow Debug Views"), this));
    auto *debugSplitter = new QSplitter(Qt::Horizontal, this);
    debugSplitter->addWidget(m_pdfExtractView);
    debugSplitter->addWidget(m_translatedView);
    debugSplitter->addWidget(m_manifestView);
    debugSplitter->setStretchFactor(0, 1);
    debugSplitter->setStretchFactor(1, 1);
    debugSplitter->setStretchFactor(2, 1);
    documentLayout->addWidget(debugSplitter, 1);
    documentTab->setLayout(documentLayout);

    auto *batchTab = new QWidget(this);
    auto *batchLayout = new QVBoxLayout(batchTab);
    m_addBatchDocumentsButton = new QPushButton(QStringLiteral("Add PDF Documents"), this);
    connect(m_addBatchDocumentsButton, &QPushButton::clicked, this, &MainWindow::onAddBatchDocumentsClicked);
    m_pauseResumeBatchButton = new QPushButton(QStringLiteral("Pause Queue"), this);
    connect(m_pauseResumeBatchButton, &QPushButton::clicked, this, &MainWindow::onPauseResumeBatchClicked);
    m_retryBatchButton = new QPushButton(QStringLiteral("Retry Selected"), this);
    connect(m_retryBatchButton, &QPushButton::clicked, this, &MainWindow::onRetrySelectedBatchClicked);
    m_openBatchResultButton = new QPushButton(QStringLiteral("Open Result"), this);
    connect(m_openBatchResultButton, &QPushButton::clicked, this, &MainWindow::onOpenSelectedBatchResultClicked);
    m_openBatchFolderButton = new QPushButton(QStringLiteral("Open Folder"), this);
    connect(m_openBatchFolderButton, &QPushButton::clicked, this, &MainWindow::onOpenSelectedBatchFolderClicked);
    m_loadBatchCompareButton = new QPushButton(QStringLiteral("Load to Compare"), this);
    connect(m_loadBatchCompareButton, &QPushButton::clicked, this, &MainWindow::onLoadSelectedBatchCompareClicked);
    m_batchStatusLabel = new QLabel(QStringLiteral("Active 0 / 2, Running"), this);
    m_batchConcurrencySpin = new QSpinBox(this);
    m_batchConcurrencySpin->setRange(1, 8);
    m_batchConcurrencySpin->setValue(2);

    m_batchTable = new QTableWidget(this);
    m_batchTable->setColumnCount(7);
    m_batchTable->setHorizontalHeaderLabels(QStringList({
        QStringLiteral("File"),
        QStringLiteral("Stage"),
        QStringLiteral("Progress"),
        QStringLiteral("Source"),
        QStringLiteral("Target"),
        QStringLiteral("Error"),
        QStringLiteral("Output")
    }));
    m_batchTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_batchTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_batchTable->setSelectionBehavior(QAbstractItemView::SelectRows);

    batchLayout->addWidget(new QLabel(QStringLiteral("Batch Background Translation Baseline"), this));
    auto *batchToolbar = new QHBoxLayout();
    batchToolbar->addWidget(m_addBatchDocumentsButton);
    batchToolbar->addWidget(m_pauseResumeBatchButton);
    batchToolbar->addWidget(m_retryBatchButton);
    batchToolbar->addWidget(m_openBatchResultButton);
    batchToolbar->addWidget(m_openBatchFolderButton);
    batchToolbar->addWidget(m_loadBatchCompareButton);
    batchToolbar->addWidget(new QLabel(QStringLiteral("Concurrency"), this));
    batchToolbar->addWidget(m_batchConcurrencySpin);
    batchToolbar->addStretch(1);
    batchToolbar->addWidget(m_batchStatusLabel);
    batchLayout->addLayout(batchToolbar);
    batchLayout->addWidget(m_batchTable, 1);
    batchTab->setLayout(batchLayout);

    auto *settingsTab = new QWidget(this);
    auto *settingsLayout = new QVBoxLayout(settingsTab);
    auto *settingsHint = new QLabel(QStringLiteral("Global runtime configuration for fragment, document and batch workflows."), this);
    settingsHint->setStyleSheet(QStringLiteral("color:#52606D;"));
    settingsLayout->addWidget(settingsHint);
    settingsLayout->addWidget(m_endpointGroup);
    settingsLayout->addWidget(m_translateEndpointGroup);
    settingsLayout->addWidget(m_extractMcpGroup);
    settingsLayout->addWidget(m_termMcpGroup);
    settingsLayout->addStretch(1);
    settingsTab->setLayout(settingsLayout);

    if (m_batchQueueController) {
        connect(m_batchQueueController.get(), &pipeline::BatchTranslationQueueController::queueChanged,
                this, [this]() {
                    refreshBatchTable();
                    saveUiState();
                });
        connect(m_batchQueueController.get(), &pipeline::BatchTranslationQueueController::pausedChanged,
                this, [this](bool) { refreshBatchTable(); });
        connect(m_batchQueueController.get(), &pipeline::BatchTranslationQueueController::activeJobsChanged,
                this, [this](int, int) { refreshBatchTable(); });
        connect(m_batchQueueController.get(), &pipeline::BatchTranslationQueueController::taskProgressChanged,
                this, [this](const QString &, const domain::DocumentTranslationTask &) {
                    refreshBatchTable();
                    saveUiState();
                });
        connect(m_batchConcurrencySpin, qOverload<int>(&QSpinBox::valueChanged),
                this, [this](int value) {
                    m_batchQueueController->setMaxConcurrentJobs(value);
                    saveUiState();
                });
    }

    m_tabs = new QTabWidget(this);
    m_tabs->addTab(fragmentTab, QStringLiteral("Fragment"));
    m_tabs->addTab(documentTab, QStringLiteral("Document"));
    m_tabs->addTab(batchTab, QStringLiteral("Batch"));
    m_tabs->addTab(settingsTab, QStringLiteral("Settings"));

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_titleLabel);
    layout->addWidget(m_statusLabel);
    layout->addWidget(m_tabs, 1);

    setLayout(layout);
    setWindowTitle(QStringLiteral("PDF Translator Agent"));
    resize(980, 720);
    refreshBatchTable();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveUiState();
    QWidget::closeEvent(event);
}

void MainWindow::loadUiState()
{
    QSettings settings;
    m_providerCombo->setCurrentText(settings.value(QStringLiteral("fragment/languageDetectProvider"), m_providerCombo->currentText()).toString());
    m_baseUrlEdit->setText(settings.value(QStringLiteral("fragment/languageDetectBaseUrl"), m_baseUrlEdit->text()).toString());
    m_apiKeyEdit->setText(settings.value(QStringLiteral("fragment/languageDetectApiKey")).toString());
    m_modelEdit->setText(settings.value(QStringLiteral("fragment/languageDetectModel")).toString());
    m_fragmentTargetLanguageCombo->setCurrentIndex(settings.value(QStringLiteral("fragment/targetLanguageIndex"), 0).toInt());
    m_fragmentInstructionsEdit->setText(settings.value(QStringLiteral("fragment/instructions")).toString());
    m_inputEdit->setPlainText(settings.value(QStringLiteral("fragment/sourceText")).toString());
    m_lastFragmentTranslation = settings.value(QStringLiteral("fragment/lastTranslation")).toString();
    m_lastFragmentSourceLanguage = settings.value(QStringLiteral("fragment/lastSourceLanguage")).toString();
    m_lastFragmentTargetLanguage = settings.value(QStringLiteral("fragment/lastTargetLanguage")).toString();
    const QJsonDocument fragmentHistoryDocument = QJsonDocument::fromJson(settings.value(QStringLiteral("fragment/history")).toByteArray());
    if (fragmentHistoryDocument.isArray()) {
        m_fragmentHistory = fragmentHistoryDocument.array();
    } else {
        m_fragmentHistory = QJsonArray();
    }
    refreshFragmentHistoryView();

    m_pdfPathEdit->setText(settings.value(QStringLiteral("document/pdfPath")).toString());
    m_extractMcpServerIdEdit->setText(settings.value(QStringLiteral("document/extractMcpServerId")).toString());
    m_extractMcpToolNameEdit->setText(settings.value(QStringLiteral("document/extractMcpToolName")).toString());
    m_termMcpServerIdEdit->setText(settings.value(QStringLiteral("document/termMcpServerId")).toString());
    m_termMcpToolNameEdit->setText(settings.value(QStringLiteral("document/termMcpToolName")).toString());
    m_translateProviderCombo->setCurrentText(settings.value(QStringLiteral("document/translateProvider"), m_translateProviderCombo->currentText()).toString());
    m_translateBaseUrlEdit->setText(settings.value(QStringLiteral("document/translateBaseUrl"), m_translateBaseUrlEdit->text()).toString());
    m_translateApiKeyEdit->setText(settings.value(QStringLiteral("document/translateApiKey")).toString());
    m_translateModelEdit->setText(settings.value(QStringLiteral("document/translateModel")).toString());
    m_batchConcurrencySpin->setValue(settings.value(QStringLiteral("batch/maxConcurrency"), m_batchConcurrencySpin->value()).toInt());
    if (m_batchQueueController) {
        const QJsonDocument batchEntriesDocument = QJsonDocument::fromJson(settings.value(QStringLiteral("batch/entries")).toByteArray());
        if (batchEntriesDocument.isArray()) {
            m_batchQueueController->restoreFromJson(batchEntriesDocument.array());
        }
        m_batchQueueController->setPaused(settings.value(QStringLiteral("batch/paused"), false).toBool());
        m_batchQueueController->setMaxConcurrentJobs(m_batchConcurrencySpin->value());
    }
    if (m_tabs) {
        m_tabs->setCurrentIndex(settings.value(QStringLiteral("ui/currentTabIndex"), 0).toInt());
    }
}

void MainWindow::saveUiState() const
{
    QSettings settings;
    settings.setValue(QStringLiteral("fragment/languageDetectProvider"), m_providerCombo->currentText());
    settings.setValue(QStringLiteral("fragment/languageDetectBaseUrl"), m_baseUrlEdit->text().trimmed());
    settings.setValue(QStringLiteral("fragment/languageDetectApiKey"), m_apiKeyEdit->text());
    settings.setValue(QStringLiteral("fragment/languageDetectModel"), m_modelEdit->text().trimmed());
    settings.setValue(QStringLiteral("fragment/targetLanguageIndex"), m_fragmentTargetLanguageCombo->currentIndex());
    settings.setValue(QStringLiteral("fragment/instructions"), m_fragmentInstructionsEdit->text());
    settings.setValue(QStringLiteral("fragment/sourceText"), m_inputEdit->toPlainText());
    settings.setValue(QStringLiteral("fragment/lastTranslation"), m_lastFragmentTranslation);
    settings.setValue(QStringLiteral("fragment/lastSourceLanguage"), m_lastFragmentSourceLanguage);
    settings.setValue(QStringLiteral("fragment/lastTargetLanguage"), m_lastFragmentTargetLanguage);
    settings.setValue(QStringLiteral("fragment/history"), QJsonDocument(m_fragmentHistory).toJson(QJsonDocument::Compact));

    settings.setValue(QStringLiteral("document/pdfPath"), m_pdfPathEdit->text().trimmed());
    settings.setValue(QStringLiteral("document/extractMcpServerId"), m_extractMcpServerIdEdit->text().trimmed());
    settings.setValue(QStringLiteral("document/extractMcpToolName"), m_extractMcpToolNameEdit->text().trimmed());
    settings.setValue(QStringLiteral("document/termMcpServerId"), m_termMcpServerIdEdit->text().trimmed());
    settings.setValue(QStringLiteral("document/termMcpToolName"), m_termMcpToolNameEdit->text().trimmed());
    settings.setValue(QStringLiteral("document/translateProvider"), m_translateProviderCombo->currentText());
    settings.setValue(QStringLiteral("document/translateBaseUrl"), m_translateBaseUrlEdit->text().trimmed());
    settings.setValue(QStringLiteral("document/translateApiKey"), m_translateApiKeyEdit->text());
    settings.setValue(QStringLiteral("document/translateModel"), m_translateModelEdit->text().trimmed());
    settings.setValue(QStringLiteral("batch/maxConcurrency"), m_batchConcurrencySpin->value());
    settings.setValue(QStringLiteral("batch/paused"), m_batchQueueController ? m_batchQueueController->isPaused() : false);
    settings.setValue(QStringLiteral("batch/entries"),
                      m_batchQueueController
                          ? QJsonDocument(m_batchQueueController->toJson()).toJson(QJsonDocument::Compact)
                          : QByteArray());
    settings.setValue(QStringLiteral("ui/currentTabIndex"), m_tabs ? m_tabs->currentIndex() : 0);
    settings.sync();
}

void MainWindow::connectPersistenceSignals()
{
    connect(m_providerCombo, &QComboBox::currentTextChanged, this, [this](const QString &) { saveUiState(); });
    connect(m_baseUrlEdit, &QLineEdit::textChanged, this, [this](const QString &) { saveUiState(); });
    connect(m_apiKeyEdit, &QLineEdit::textChanged, this, [this](const QString &) { saveUiState(); });
    connect(m_modelEdit, &QLineEdit::textChanged, this, [this](const QString &) { saveUiState(); });
    connect(m_fragmentTargetLanguageCombo, &QComboBox::currentTextChanged, this, [this](const QString &) { saveUiState(); });
    connect(m_fragmentInstructionsEdit, &QLineEdit::textChanged, this, [this](const QString &) { saveUiState(); });
    connect(m_inputEdit, &QPlainTextEdit::textChanged, this, [this]() { saveUiState(); });
    connect(m_pdfPathEdit, &QLineEdit::textChanged, this, [this](const QString &) { saveUiState(); });
    connect(m_translateProviderCombo, &QComboBox::currentTextChanged, this, [this](const QString &) { saveUiState(); });
    connect(m_translateBaseUrlEdit, &QLineEdit::textChanged, this, [this](const QString &) { saveUiState(); });
    connect(m_translateApiKeyEdit, &QLineEdit::textChanged, this, [this](const QString &) { saveUiState(); });
    connect(m_translateModelEdit, &QLineEdit::textChanged, this, [this](const QString &) { saveUiState(); });
    connect(m_extractMcpServerIdEdit, &QLineEdit::textChanged, this, [this](const QString &) { saveUiState(); });
    connect(m_extractMcpToolNameEdit, &QLineEdit::textChanged, this, [this](const QString &) { saveUiState(); });
    connect(m_termMcpServerIdEdit, &QLineEdit::textChanged, this, [this](const QString &) { saveUiState(); });
    connect(m_termMcpToolNameEdit, &QLineEdit::textChanged, this, [this](const QString &) { saveUiState(); });
    connect(m_tabs, &QTabWidget::currentChanged, this, [this](int) { saveUiState(); });
}

void MainWindow::appendFragmentHistoryEntry(const QString &role, const QString &text)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }

    QJsonObject item;
    item.insert(QStringLiteral("role"), role);
    item.insert(QStringLiteral("text"), trimmed);
    m_fragmentHistory.append(item);

    while (m_fragmentHistory.size() > 20) {
        m_fragmentHistory.removeFirst();
    }

    refreshFragmentHistoryView();
    saveUiState();
}

void MainWindow::refreshFragmentHistoryView()
{
    if (!m_fragmentHistoryView) {
        return;
    }

    if (m_fragmentHistory.isEmpty()) {
        m_fragmentHistoryView->clear();
        return;
    }

    m_fragmentHistoryView->setHtml(fragmentHistoryToHtml(m_fragmentHistory));
}

void MainWindow::applyLanguageDetectBinding()
{
    if (!m_modelRouter) {
        return;
    }

    skills::ModelEndpointConfig endpoint;
    endpoint.id = QStringLiteral("language-detect-default");
    endpoint.displayName = QStringLiteral("Language Detect Endpoint");
    endpoint.llmConfig.providerName = m_providerCombo->currentText().trimmed();
    endpoint.llmConfig.baseUrl = m_baseUrlEdit->text().trimmed();
    endpoint.llmConfig.apiKey = m_apiKeyEdit->text().trimmed();
    endpoint.llmConfig.model = m_modelEdit->text().trimmed();
    endpoint.llmConfig.stream = false;
    endpoint.llmConfig.timeoutMs = 30000;
    m_modelRouter->upsertEndpoint(endpoint);

    skills::SkillExecutionBinding binding;
    binding.skillId = QStringLiteral("language-detect");
    binding.endpointId = endpoint.id;
    binding.executionType = skills::SkillExecutionType::Hybrid;
    m_modelRouter->upsertBinding(binding);
}

void MainWindow::applyTranslateBinding()
{
    if (!m_modelRouter) {
        return;
    }

    skills::ModelEndpointConfig endpoint;
    endpoint.id = QStringLiteral("chunk-translate-default");
    endpoint.displayName = QStringLiteral("Chunk Translate Endpoint");
    endpoint.llmConfig.providerName = m_translateProviderCombo->currentText().trimmed();
    endpoint.llmConfig.baseUrl = m_translateBaseUrlEdit->text().trimmed();
    endpoint.llmConfig.apiKey = m_translateApiKeyEdit->text().trimmed();
    endpoint.llmConfig.model = m_translateModelEdit->text().trimmed();
    endpoint.llmConfig.stream = false;
    endpoint.llmConfig.timeoutMs = 120000;
    m_modelRouter->upsertEndpoint(endpoint);

    skills::SkillExecutionBinding binding;
    binding.skillId = QStringLiteral("chunk-translate");
    binding.endpointId = endpoint.id;
    binding.executionType = skills::SkillExecutionType::Llm;
    m_modelRouter->upsertBinding(binding);
}

void MainWindow::applyPdfExtractBinding()
{
    if (!m_modelRouter) {
        return;
    }

    const QString serverId = m_extractMcpServerIdEdit->text().trimmed();
    const QString toolName = m_extractMcpToolNameEdit->text().trimmed();
    if (serverId.isEmpty() || toolName.isEmpty()) {
        return;
    }

    skills::SkillExecutionBinding binding;
    binding.skillId = QStringLiteral("pdf-text-extract");
    binding.executionType = skills::SkillExecutionType::Mcp;
    binding.mcpServerId = serverId;
    binding.mcpToolName = toolName;
    m_modelRouter->upsertBinding(binding);
}

void MainWindow::applyTermBaseBinding()
{
    if (!m_modelRouter) {
        return;
    }

    const QString serverId = m_termMcpServerIdEdit->text().trimmed();
    const QString toolName = m_termMcpToolNameEdit->text().trimmed();
    if (serverId.isEmpty() || toolName.isEmpty()) {
        return;
    }

    skills::SkillExecutionBinding binding;
    binding.skillId = QStringLiteral("term-base-resolve");
    binding.executionType = skills::SkillExecutionType::Mcp;
    binding.mcpServerId = serverId;
    binding.mcpToolName = toolName;
    m_modelRouter->upsertBinding(binding);
}

QString MainWindow::defaultManifestPathForPdf(const QString &pdfPath) const
{
    const QFileInfo info(pdfPath);
    return info.absolutePath()
        + QDir::separator()
        + info.completeBaseName()
        + QStringLiteral(".translation.manifest.json");
}

int MainWindow::selectedBatchRow() const
{
    return m_batchTable ? m_batchTable->currentRow() : -1;
}

} // namespace pdftranslator
