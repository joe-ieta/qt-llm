#include "mcpchatwindow.h"

#include "../../qtllm/chat/conversationclient.h"
#include "../../qtllm/core/llmconfig.h"
#include "../../qtllm/logging/qtllmlogger.h"
#include "../../qtllm/logging/signallogsink.h"
#include "../../qtllm/logging/logtypes.h"
#include "../../qtllm/runtime/managedllamacppruntime.h"
#include "../../qtllm/tools/llmtooldefinition.h"
#include "../../qtllm/tools/llmtoolregistry.h"
#include "../../qtllm/tools/toolenabledchatentry.h"
#include "../../qtllm/tools/mcp/mcptoolsyncservice.h"
#include "../../qtllm/tools/runtime/toolexecutionlayer.h"
#include "../../qtllm/tools/runtime/toolruntimehooks.h"
#include "../../qtllm/tools/runtime/toolruntime_types.h"

#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QListWidget>
#include <QPushButton>
#include <QSplitter>
#include <QTabWidget>
#include <QTextCursor>
#include <QTextEdit>
#include <QUuid>
#include <QVBoxLayout>

#include <functional>

namespace {

QString defaultBaseUrlForProvider(const QString &provider)
{
    const QString normalized = provider.trimmed().toLower();
    if (qtllm::runtime::ManagedLlamaCppRuntime::isManagedProvider(normalized)) {
        return qtllm::runtime::ManagedLlamaCppRuntime::defaultBaseUrl();
    }
    if (normalized == QStringLiteral("ollama")) {
        return QStringLiteral("http://127.0.0.1:11434/v1");
    }
    if (normalized == QStringLiteral("openai")) {
        return QStringLiteral("https://api.openai.com/v1");
    }
    return QStringLiteral("http://127.0.0.1:8000/v1");
}

QString jsonToCompactText(const QJsonObject &object)
{
    return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact));
}

QString selectedModelPath(QComboBox *combo)
{
    if (!combo) {
        return {};
    }
    const int index = combo->findText(combo->currentText().trimmed());
    return index >= 0 ? combo->itemData(index).toString() : QString();
}

QString maskSecret(const QString &value)
{
    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty()) {
        return QStringLiteral("<empty>");
    }
    if (trimmed.size() <= 6) {
        return QStringLiteral("***");
    }
    return trimmed.left(3) + QStringLiteral("***") + trimmed.right(3);
}

QString formatLogEvent(const qtllm::logging::LogEvent &event)
{
    QString line = QStringLiteral("[%1][%2][%3]")
                       .arg(event.timestampUtc.toString(QStringLiteral("HH:mm:ss.zzz")),
                            qtllm::logging::logLevelToString(event.level),
                            event.category);
    if (!event.clientId.isEmpty()) {
        line += QStringLiteral("[client=%1]").arg(event.clientId);
    }
    if (!event.sessionId.isEmpty()) {
        line += QStringLiteral("[session=%1]").arg(event.sessionId);
    }
    if (!event.requestId.isEmpty()) {
        line += QStringLiteral("[request=%1]").arg(event.requestId);
    }
    line += QStringLiteral(" ") + event.message;
    if (!event.fields.isEmpty()) {
        line += QStringLiteral(" ") + jsonToCompactText(event.fields);
    }
    return line;
}

void appendPlainText(QTextEdit *edit, const QString &text)
{
    if (!edit || text.isEmpty()) {
        return;
    }

    QTextCursor cursor = edit->textCursor();
    cursor.movePosition(QTextCursor::End);
    cursor.insertText(text);
    edit->setTextCursor(cursor);
    edit->ensureCursorVisible();
}

class DemoToolRuntimeHooks : public qtllm::tools::runtime::ToolRuntimeHooks
{
public:
    explicit DemoToolRuntimeHooks(std::function<void(const QString &, const QString &, const QJsonObject &)> logger)
        : m_logger(std::move(logger))
    {
    }

    void beforeExecute(const qtllm::tools::runtime::ToolCallRequest &request,
                       const qtllm::tools::runtime::ToolExecutionContext &context) override
    {
        QJsonObject fields;
        fields.insert(QStringLiteral("toolId"), request.toolId);
        fields.insert(QStringLiteral("sessionId"), context.sessionId);
        fields.insert(QStringLiteral("arguments"), request.arguments);
        if (m_logger) {
            m_logger(QStringLiteral("ui.mcp.chat"), QStringLiteral("Tool execution started"), fields);
        }
    }

    void afterExecute(const qtllm::tools::runtime::ToolExecutionResult &result,
                      const qtllm::tools::runtime::ToolExecutionContext &context) override
    {
        Q_UNUSED(context)
        if (!m_logger) {
            return;
        }

        QJsonObject fields;
        fields.insert(QStringLiteral("toolId"), result.toolId);
        fields.insert(QStringLiteral("callId"), result.callId);
        fields.insert(QStringLiteral("success"), result.success);
        fields.insert(QStringLiteral("durationMs"), static_cast<qint64>(result.durationMs));
        fields.insert(QStringLiteral("errorCode"), result.errorCode);
        if (result.success) {
            fields.insert(QStringLiteral("output"), result.output);
        } else {
            fields.insert(QStringLiteral("errorMessage"), result.errorMessage);
        }
        m_logger(QStringLiteral("ui.mcp.chat"), QStringLiteral("Tool execution finished"), fields);
    }

private:
    std::function<void(const QString &, const QString &, const QJsonObject &)> m_logger;
};

} // namespace

McpChatWindow::McpChatWindow(std::shared_ptr<qtllm::tools::mcp::McpServerManager> serverManager,
                             std::shared_ptr<qtllm::tools::mcp::IMcpClient> mcpClient,
                             QWidget *parent)
    : QWidget(parent)
    , m_serverManager(std::move(serverManager))
    , m_mcpClient(std::move(mcpClient))
    , m_toolRegistry(std::make_shared<qtllm::tools::LlmToolRegistry>())
    , m_toolSyncService(std::make_shared<qtllm::tools::mcp::McpToolSyncService>(
          m_toolRegistry,
          m_serverManager ? m_serverManager->registry() : std::shared_ptr<qtllm::tools::mcp::McpServerRegistry>(),
          m_mcpClient))
    , m_executionLayer(std::make_shared<qtllm::tools::runtime::ToolExecutionLayer>())
    , m_logSink(std::make_shared<qtllm::logging::SignalLogSink>())
    , m_conversationClient(QSharedPointer<qtllm::chat::ConversationClient>::create(
          QStringLiteral("mcp-chat-") + QUuid::createUuid().toString(QUuid::WithoutBraces)))
    , m_providerCombo(new QComboBox(this))
    , m_baseUrlEdit(new QLineEdit(this))
    , m_apiKeyEdit(new QLineEdit(this))
    , m_modelCombo(new QComboBox(this))
    , m_applyConfigButton(new QPushButton(QStringLiteral("Apply Config"), this))
    , m_syncToolsButton(new QPushButton(QStringLiteral("Reload MCP Tools"), this))
    , m_toolCountLabel(new QLabel(this))
    , m_registeredServersView(new QListWidget(this))
    , m_allToolsView(new QTextEdit(this))
    , m_selectedToolsView(new QTextEdit(this))
    , m_toolSchemaView(new QTextEdit(this))
    , m_requestPayloadView(new QTextEdit(this))
    , m_chatOutput(new QTextEdit(this))
    , m_reasoningOutput(new QTextEdit(this))
    , m_inputEdit(new QTextEdit(this))
    , m_sendButton(new QPushButton(QStringLiteral("Send"), this))
    , m_clearChatButton(new QPushButton(QStringLiteral("Clear Chat"), this))
    , m_logLevelFilter(new QComboBox(this))
    , m_logCategoryFilter(new QLineEdit(this))
    , m_logClientIdFilter(new QLineEdit(this))
    , m_logSessionIdFilter(new QLineEdit(this))
    , m_logRequestIdFilter(new QLineEdit(this))
    , m_runtimeLog(new QTextEdit(this))
{
    setAttribute(Qt::WA_DeleteOnClose, true);
    setWindowTitle(QStringLiteral("MCP Tools Enabled Chat"));
    resize(1680, 1080);

    qtllm::logging::QtLlmLogger::instance().addSink(m_logSink);
    connect(m_logSink.get(), &qtllm::logging::SignalLogSink::logEventReceived,
            this, &McpChatWindow::onLogEventReceived);

    m_providerCombo->addItem(QStringLiteral("llama-cpp"));
    m_providerCombo->addItem(QStringLiteral("ollama"));
    m_providerCombo->addItem(QStringLiteral("openai-compatible"));
    m_providerCombo->addItem(QStringLiteral("vllm"));
    m_providerCombo->addItem(QStringLiteral("openai"));
    m_baseUrlEdit->setText(defaultBaseUrlForProvider(m_providerCombo->currentText()));
    m_apiKeyEdit->setEchoMode(QLineEdit::Password);
    m_modelCombo->setEditable(true);
    m_modelCombo->setInsertPolicy(QComboBox::NoInsert);
    if (m_modelCombo->lineEdit()) {
        m_modelCombo->lineEdit()->setPlaceholderText(QStringLiteral("Model name"));
    }
    m_inputEdit->setPlaceholderText(QStringLiteral("Enter a message that should trigger one of the MCP tools"));

    m_allToolsView->setReadOnly(true);
    m_selectedToolsView->setReadOnly(true);
    m_toolSchemaView->setReadOnly(true);
    m_requestPayloadView->setReadOnly(true);
    m_chatOutput->setReadOnly(true);
    m_reasoningOutput->setReadOnly(true);
    m_runtimeLog->setReadOnly(true);
    m_logLevelFilter->addItems(QStringList({QStringLiteral("debug+"), QStringLiteral("info+"), QStringLiteral("warn+"), QStringLiteral("error") }));
    m_logCategoryFilter->setPlaceholderText(QStringLiteral("category contains..."));
    m_logClientIdFilter->setPlaceholderText(QStringLiteral("clientId contains..."));
    m_logSessionIdFilter->setPlaceholderText(QStringLiteral("sessionId contains..."));
    m_logRequestIdFilter->setPlaceholderText(QStringLiteral("requestId contains..."));

    auto *configForm = new QFormLayout();
    configForm->addRow(QStringLiteral("Provider"), m_providerCombo);
    configForm->addRow(QStringLiteral("Base URL"), m_baseUrlEdit);
    configForm->addRow(QStringLiteral("API Key"), m_apiKeyEdit);
    configForm->addRow(QStringLiteral("Model"), m_modelCombo);

    auto *configButtons = new QHBoxLayout();
    configButtons->addWidget(m_applyConfigButton);
    configButtons->addWidget(m_syncToolsButton);

    auto *leftTopLayout = new QVBoxLayout();
    leftTopLayout->addLayout(configForm);
    leftTopLayout->addLayout(configButtons);
    leftTopLayout->addWidget(new QLabel(QStringLiteral("Registered MCP Servers"), this));
    leftTopLayout->addWidget(m_registeredServersView);
    leftTopLayout->addWidget(m_toolCountLabel);
    auto *leftTopPane = new QWidget(this);
    leftTopPane->setLayout(leftTopLayout);

    auto *leftBottomLayout = new QVBoxLayout();
    leftBottomLayout->addWidget(new QLabel(QStringLiteral("Current Tool Registry"), this));
    leftBottomLayout->addWidget(m_allToolsView);
    auto *leftBottomPane = new QWidget(this);
    leftBottomPane->setLayout(leftBottomLayout);

    auto *leftSplitter = new QSplitter(Qt::Vertical, this);
    leftSplitter->addWidget(leftTopPane);
    leftSplitter->addWidget(leftBottomPane);
    leftSplitter->setStretchFactor(0, 1);
    leftSplitter->setStretchFactor(1, 1);

    auto *chatButtons = new QHBoxLayout();
    chatButtons->addWidget(m_sendButton);
    chatButtons->addWidget(m_clearChatButton);

    auto *outputsSplitter = new QSplitter(Qt::Vertical, this);
    outputsSplitter->addWidget(m_chatOutput);
    outputsSplitter->addWidget(m_reasoningOutput);
    outputsSplitter->setStretchFactor(0, 2);
    outputsSplitter->setStretchFactor(1, 1);

    auto *chatPaneLayout = new QVBoxLayout();
    chatPaneLayout->addWidget(new QLabel(QStringLiteral("Assistant Content"), this));
    chatPaneLayout->addWidget(outputsSplitter, 1);
    chatPaneLayout->addWidget(new QLabel(QStringLiteral("Input"), this));
    chatPaneLayout->addWidget(m_inputEdit);
    chatPaneLayout->addLayout(chatButtons);
    auto *chatPane = new QWidget(this);
    chatPane->setLayout(chatPaneLayout);

    auto *logFilterRow = new QHBoxLayout();
    logFilterRow->addWidget(new QLabel(QStringLiteral("Level"), this));
    logFilterRow->addWidget(m_logLevelFilter);
    logFilterRow->addWidget(new QLabel(QStringLiteral("Category"), this));
    logFilterRow->addWidget(m_logCategoryFilter, 1);
    logFilterRow->addWidget(new QLabel(QStringLiteral("Client"), this));
    logFilterRow->addWidget(m_logClientIdFilter, 1);
    logFilterRow->addWidget(new QLabel(QStringLiteral("Session"), this));
    logFilterRow->addWidget(m_logSessionIdFilter, 1);
    logFilterRow->addWidget(new QLabel(QStringLiteral("Request"), this));
    logFilterRow->addWidget(m_logRequestIdFilter, 1);

    auto *runtimeLogLayout = new QVBoxLayout();
    runtimeLogLayout->addLayout(logFilterRow);
    runtimeLogLayout->addWidget(m_runtimeLog, 1);
    auto *runtimeLogPane = new QWidget(this);
    runtimeLogPane->setLayout(runtimeLogLayout);

    auto *inspectTabs = new QTabWidget(this);
    inspectTabs->addTab(m_selectedToolsView, QStringLiteral("Selected Tools"));
    inspectTabs->addTab(m_toolSchemaView, QStringLiteral("Tools Schema"));
    inspectTabs->addTab(m_requestPayloadView, QStringLiteral("Request Payload"));
    inspectTabs->addTab(runtimeLogPane, QStringLiteral("Runtime Log"));

    auto *rightSplitter = new QSplitter(Qt::Vertical, this);
    rightSplitter->addWidget(chatPane);
    rightSplitter->addWidget(inspectTabs);
    rightSplitter->setStretchFactor(0, 3);
    rightSplitter->setStretchFactor(1, 2);

    auto *mainSplitter = new QSplitter(Qt::Horizontal, this);
    mainSplitter->addWidget(leftSplitter);
    mainSplitter->addWidget(rightSplitter);
    mainSplitter->setStretchFactor(0, 1);
    mainSplitter->setStretchFactor(1, 2);

    auto *rootLayout = new QHBoxLayout(this);
    rootLayout->addWidget(mainSplitter);
    setLayout(rootLayout);

    m_executionLayer->setMcpClient(m_mcpClient);
    if (m_serverManager) {
        m_executionLayer->setMcpServerRegistry(m_serverManager->registry());
    }
    m_executionLayer->setHooks(std::make_shared<DemoToolRuntimeHooks>(
        [this](const QString &category, const QString &message, const QJsonObject &fields) {
            logInfo(category, message, fields);
        }));

    m_chatEntry = new qtllm::tools::ToolEnabledChatEntry(m_conversationClient, m_toolRegistry, this);
    m_chatEntry->setExecutionLayer(m_executionLayer);
    m_chatEntry->setMcpClient(m_mcpClient);
    if (m_serverManager) {
        m_chatEntry->setMcpServerRegistry(m_serverManager->registry());
    }

    connect(m_providerCombo,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            [this](int) {
                const QString provider = m_providerCombo->currentText();
                m_baseUrlEdit->setText(defaultBaseUrlForProvider(provider));
                refreshLocalModelList();
                logInfo(QStringLiteral("ui.mcp.chat"),
                        QStringLiteral("Provider selection changed"),
                        QJsonObject{{QStringLiteral("provider"), provider},
                                    {QStringLiteral("defaultBaseUrl"), m_baseUrlEdit->text()}});
            });
    connect(m_applyConfigButton, &QPushButton::clicked, this, &McpChatWindow::onApplyConfigClicked);
    connect(m_syncToolsButton, &QPushButton::clicked, this, &McpChatWindow::onSyncToolsClicked);
    connect(m_sendButton, &QPushButton::clicked, this, &McpChatWindow::onSendClicked);
    connect(m_clearChatButton, &QPushButton::clicked, this, &McpChatWindow::onClearChatClicked);
    connect(m_chatEntry,
            &qtllm::tools::ToolEnabledChatEntry::tokenReceived,
            this,
            &McpChatWindow::onContentTokenReceived);
    connect(m_chatEntry,
            &qtllm::tools::ToolEnabledChatEntry::reasoningTokenReceived,
            this,
            &McpChatWindow::onReasoningTokenReceived);
    connect(m_chatEntry,
            &qtllm::tools::ToolEnabledChatEntry::toolSelectionPrepared,
            this,
            &McpChatWindow::onToolSelectionPrepared);
    connect(m_chatEntry,
            &qtllm::tools::ToolEnabledChatEntry::toolSchemaPrepared,
            this,
            &McpChatWindow::onToolSchemaPrepared);
    connect(m_conversationClient.get(),
            &qtllm::chat::ConversationClient::requestPrepared,
            this,
            &McpChatWindow::onRequestPrepared);
    connect(m_conversationClient.get(),
            &qtllm::chat::ConversationClient::providerPayloadPrepared,
            this,
            &McpChatWindow::onProviderPayloadPrepared);
    connect(m_chatEntry,
            &qtllm::tools::ToolEnabledChatEntry::completed,
            this,
            &McpChatWindow::onChatCompleted);
    connect(m_chatEntry,
            &qtllm::tools::ToolEnabledChatEntry::errorOccurred,
            this,
            &McpChatWindow::onChatError);

    refreshLocalModelList();
    applyChatConfig(false);
    reloadRegisteredServers();
}

McpChatWindow::~McpChatWindow()
{
    qtllm::logging::QtLlmLogger::instance().removeSink(m_logSink);
}

void McpChatWindow::reloadRegisteredServers()
{
    QString errorMessage;
    if (m_serverManager && !m_serverManager->load(&errorMessage) && !errorMessage.isEmpty()) {
        logError(QStringLiteral("ui.mcp.chat"), QStringLiteral("Failed to reload registered MCP servers"),
                 QJsonObject{{QStringLiteral("error"), errorMessage}});
    }

    renderRegisteredServers();
    syncRegisteredTools();
}

void McpChatWindow::onApplyConfigClicked()
{
    applyChatConfig(true);
}

void McpChatWindow::onSyncToolsClicked()
{
    logInfo(QStringLiteral("ui.mcp.chat"), QStringLiteral("Manual MCP tool sync requested"));
    syncRegisteredTools();
}

void McpChatWindow::onSendClicked()
{
    const QString text = m_inputEdit->toPlainText().trimmed();
    if (text.isEmpty()) {
        logWarn(QStringLiteral("ui.mcp.chat"), QStringLiteral("Send skipped because input is empty"));
        return;
    }

    if (!applyChatConfig(true)) {
        return;
    }

    syncRegisteredTools();
    appendChatLine(QStringLiteral("user"), text);
    if (!m_reasoningOutput->toPlainText().trimmed().isEmpty()) {
        m_reasoningOutput->append(QString());
    }
    m_reasoningOutput->append(QStringLiteral("[assistant reasoning]"));
    m_assistantContentTurnActive = false;
    m_reasoningTurnActive = false;
    logInfo(QStringLiteral("ui.mcp.chat"),
            QStringLiteral("Sending chat turn"),
            QJsonObject{{QStringLiteral("textLength"), text.size()},
                        {QStringLiteral("registryEnabledTools"), m_toolRegistry ? m_toolRegistry->enabledTools().size() : 0},
                        {QStringLiteral("sessionId"), m_conversationClient ? m_conversationClient->activeSessionId() : QStringLiteral("<none>")}});
    m_inputEdit->clear();
    m_chatEntry->sendUserMessage(text);
}

void McpChatWindow::onClearChatClicked()
{
    if (m_conversationClient) {
        m_conversationClient->clearHistory();
    }
    m_chatOutput->clear();
    m_reasoningOutput->clear();
    m_selectedToolsView->clear();
    m_toolSchemaView->clear();
    m_requestPayloadView->clear();
    m_assistantContentTurnActive = false;
    m_reasoningTurnActive = false;
    logInfo(QStringLiteral("ui.mcp.chat"), QStringLiteral("Chat history cleared"));
}

void McpChatWindow::onContentTokenReceived(const QString &token)
{
    if (token.isEmpty()) {
        return;
    }

    if (!m_assistantContentTurnActive) {
        if (!m_chatOutput->toPlainText().trimmed().isEmpty()) {
            m_chatOutput->append(QString());
        }
        appendPlainText(m_chatOutput, QStringLiteral("[assistant]\n"));
        m_assistantContentTurnActive = true;
    }

    appendPlainText(m_chatOutput, token);
}

void McpChatWindow::onReasoningTokenReceived(const QString &token)
{
    if (token.isEmpty()) {
        return;
    }
    m_reasoningTurnActive = true;
    appendPlainText(m_reasoningOutput, token);
}

void McpChatWindow::onToolSelectionPrepared(const QStringList &toolIds)
{
    if (toolIds.isEmpty()) {
        m_selectedToolsView->setPlainText(QStringLiteral("No tools selected for this turn."));
        return;
    }

    m_selectedToolsView->setPlainText(toolIds.join(QStringLiteral("\n")));
}

void McpChatWindow::onToolSchemaPrepared(const QString &schemaText)
{
    const QString trimmed = schemaText.trimmed();
    if (trimmed.isEmpty() || trimmed == QStringLiteral("[]")) {
        m_toolSchemaView->setPlainText(QStringLiteral("[]"));
        return;
    }

    m_toolSchemaView->setPlainText(schemaText);
}

void McpChatWindow::onRequestPrepared(const QString &payloadJson)
{
    m_requestPayloadView->setPlainText(payloadJson);
}

void McpChatWindow::onChatCompleted(const QString &text)
{
    if (m_assistantContentTurnActive) {
        appendPlainText(m_chatOutput, QStringLiteral("\n"));
    } else if (!text.trimmed().isEmpty()) {
        appendChatLine(QStringLiteral("assistant"), text);
    }
    m_assistantContentTurnActive = false;
    if (!m_reasoningTurnActive) {
        appendPlainText(m_reasoningOutput, QStringLiteral("\n(no reasoning emitted)\n"));
    } else {
        appendPlainText(m_reasoningOutput, QStringLiteral("\n"));
    }
    m_reasoningTurnActive = false;
}

void McpChatWindow::onChatError(const QString &message)
{
    m_assistantContentTurnActive = false;
    m_reasoningTurnActive = false;
    logError(QStringLiteral("ui.mcp.chat"), QStringLiteral("Chat error surfaced to window"),
             QJsonObject{{QStringLiteral("error"), message}});
}

void McpChatWindow::onLogEventReceived(const qtllm::logging::LogEvent &event)
{
    if (!m_conversationClient || !shouldDisplayLogEvent(event)) {
        return;
    }

    const QString clientId = m_conversationClient->uid();
    const bool belongsToClient = !event.clientId.isEmpty() && event.clientId == clientId;
    const bool chatUiEvent = event.category.startsWith(QStringLiteral("ui.mcp.chat"));
    const bool chatInfraEvent = event.category.startsWith(QStringLiteral("tool.registry"))
        || event.category.startsWith(QStringLiteral("mcp.sync"));
    if (!belongsToClient && !chatUiEvent && !chatInfraEvent) {
        return;
    }

    appendRuntimeLog(formatLogEvent(event));
}

bool McpChatWindow::shouldDisplayLogEvent(const qtllm::logging::LogEvent &event) const
{
    if (!m_logLevelFilter) {
        return true;
    }

    int minimumLevel = static_cast<int>(qtllm::logging::LogLevel::Debug);
    const QString levelFilter = m_logLevelFilter->currentText().trimmed().toLower();
    if (levelFilter == QStringLiteral("info+")) {
        minimumLevel = static_cast<int>(qtllm::logging::LogLevel::Info);
    } else if (levelFilter == QStringLiteral("warn+")) {
        minimumLevel = static_cast<int>(qtllm::logging::LogLevel::Warn);
    } else if (levelFilter == QStringLiteral("error")) {
        minimumLevel = static_cast<int>(qtllm::logging::LogLevel::Error);
    }

    if (static_cast<int>(event.level) < minimumLevel) {
        return false;
    }

    const QString categoryFilter = m_logCategoryFilter ? m_logCategoryFilter->text().trimmed() : QString();
    if (!categoryFilter.isEmpty() && !event.category.contains(categoryFilter, Qt::CaseInsensitive)) {
        return false;
    }


    const QString clientIdFilter = m_logClientIdFilter ? m_logClientIdFilter->text().trimmed() : QString();
    if (!clientIdFilter.isEmpty() && !event.clientId.contains(clientIdFilter, Qt::CaseInsensitive)) {
        return false;
    }

    const QString sessionIdFilter = m_logSessionIdFilter ? m_logSessionIdFilter->text().trimmed() : QString();
    if (!sessionIdFilter.isEmpty() && !event.sessionId.contains(sessionIdFilter, Qt::CaseInsensitive)) {
        return false;
    }

    const QString requestIdFilter = m_logRequestIdFilter ? m_logRequestIdFilter->text().trimmed() : QString();
    if (!requestIdFilter.isEmpty() && !event.requestId.contains(requestIdFilter, Qt::CaseInsensitive)) {
        return false;
    }

    return true;
}

bool McpChatWindow::applyChatConfig(bool logResult)
{
    if (!m_conversationClient) {
        return false;
    }

    const QString provider = m_providerCombo->currentText().trimmed().toLower();
    QString baseUrl = m_baseUrlEdit->text().trimmed();
    QString model = m_modelCombo->currentText().trimmed();
    QString modelPath = selectedModelPath(m_modelCombo);

    if (baseUrl.isEmpty()) {
        baseUrl = defaultBaseUrlForProvider(provider);
        m_baseUrlEdit->setText(baseUrl);
    }

    if (model.isEmpty() && qtllm::runtime::ManagedLlamaCppRuntime::isManagedProvider(provider)) {
        refreshLocalModelList();
        model = m_modelCombo->currentText().trimmed();
        modelPath = selectedModelPath(m_modelCombo);
    }

    if (model.isEmpty()) {
        if (logResult) {
            logWarn(QStringLiteral("ui.mcp.chat"), QStringLiteral("Chat config invalid because model is empty"));
        }
        return false;
    }

    qtllm::LlmConfig config;
    config.providerName = provider;
    config.baseUrl = baseUrl;
    config.apiKey = m_apiKeyEdit->text().trimmed();
    config.model = model;
    config.stream = true;
    if (qtllm::runtime::ManagedLlamaCppRuntime::isManagedProvider(provider)) {
        config.runtimeName = QStringLiteral("llama-cpp-managed");
        if (!modelPath.isEmpty()) {
            config.llamaCppModelPath = modelPath;
        }
    }

    m_conversationClient->setConfig(config);
    if (!m_conversationClient->setProviderByName(provider)) {
        if (logResult) {
            logError(QStringLiteral("ui.mcp.chat"), QStringLiteral("Unsupported provider configured"),
                     QJsonObject{{QStringLiteral("provider"), provider}});
        }
        return false;
    }

    if (logResult) {
        logInfo(QStringLiteral("ui.mcp.chat"),
                QStringLiteral("Chat config applied"),
                QJsonObject{{QStringLiteral("provider"), provider},
                            {QStringLiteral("baseUrl"), baseUrl},
                            {QStringLiteral("model"), model},
                            {QStringLiteral("apiKey"), maskSecret(config.apiKey)},
                            {QStringLiteral("endpointHint"),
                             provider == QStringLiteral("openai")
                                 ? (baseUrl.endsWith(QStringLiteral("/responses"))
                                        ? baseUrl
                                        : baseUrl + QStringLiteral("/responses"))
                                 : (baseUrl.endsWith(QStringLiteral("/chat/completions"))
                                        ? baseUrl
                                        : baseUrl + QStringLiteral("/chat/completions"))}});
    }

    return true;
}

void McpChatWindow::refreshLocalModelList()
{
    const QString provider = m_providerCombo->currentText().trimmed();
    if (!qtllm::runtime::ManagedLlamaCppRuntime::isManagedProvider(provider)) {
        return;
    }

    const QString previousModel = m_modelCombo->currentText();
    const QString previousModelPath = m_modelCombo->currentData().toString();

    qtllm::LlmConfig config;
    config.providerName = provider;

    qtllm::runtime::LlamaCppRuntimeLayout layout;
    QString errorMessage;
    const QList<qtllm::runtime::LlamaCppLocalModel> models =
        qtllm::runtime::ManagedLlamaCppRuntime::listLocalModels(config, &layout, &errorMessage);

    m_modelCombo->clear();
    if (!errorMessage.isEmpty()) {
        logWarn(QStringLiteral("ui.mcp.chat"), errorMessage);
        return;
    }

    for (const qtllm::runtime::LlamaCppLocalModel &model : models) {
        m_modelCombo->addItem(model.displayName, model.filePath);
    }

    int previousIndex = previousModelPath.isEmpty() ? -1 : m_modelCombo->findData(previousModelPath);
    if (previousIndex < 0) {
        previousIndex = m_modelCombo->findText(previousModel);
    }
    if (previousIndex >= 0) {
        m_modelCombo->setCurrentIndex(previousIndex);
    } else if (m_modelCombo->count() > 0) {
        m_modelCombo->setCurrentIndex(0);
    }

    logInfo(QStringLiteral("ui.mcp.chat"),
            QStringLiteral("Local llama.cpp model list refreshed"),
            QJsonObject{{QStringLiteral("modelsDir"), layout.modelsDir},
                        {QStringLiteral("modelCount"), m_modelCombo->count()}});
}

void McpChatWindow::syncRegisteredTools()
{
    if (!m_toolRegistry) {
        logWarn(QStringLiteral("ui.mcp.chat"), QStringLiteral("Tool sync skipped because registry is missing"));
        return;
    }

    m_toolRegistry->clear();

    int syncedServerCount = 0;
    int failedServerCount = 0;

    if (m_serverManager) {
        const QVector<qtllm::tools::mcp::McpServerDefinition> servers = m_serverManager->allServers();
        logInfo(QStringLiteral("ui.mcp.chat"),
                QStringLiteral("Syncing registered MCP servers into tool registry"),
                QJsonObject{{QStringLiteral("registeredServerCount"), servers.size()}});

        for (const qtllm::tools::mcp::McpServerDefinition &server : servers) {
            if (!server.enabled) {
                continue;
            }

            QString errorMessage;
            if (m_toolSyncService && m_toolSyncService->syncServerTools(server.serverId, &errorMessage)) {
                ++syncedServerCount;
            } else {
                ++failedServerCount;
                logWarn(QStringLiteral("ui.mcp.chat"),
                        QStringLiteral("MCP server tool sync failed"),
                        QJsonObject{{QStringLiteral("serverId"), server.serverId},
                                    {QStringLiteral("error"), errorMessage}});
            }
        }
    }

    renderAllTools();

    int mcpToolCount = 0;
    const QList<qtllm::tools::LlmToolDefinition> allTools = m_toolRegistry->allTools();
    for (const qtllm::tools::LlmToolDefinition &tool : allTools) {
        if (tool.category == QStringLiteral("mcp")) {
            ++mcpToolCount;
        }
    }

    m_toolCountLabel->setText(QStringLiteral("Registry tools: %1 total, %2 MCP, %3 servers synced, %4 failed")
                                  .arg(QString::number(allTools.size()),
                                       QString::number(mcpToolCount),
                                       QString::number(syncedServerCount),
                                       QString::number(failedServerCount)));
    logInfo(QStringLiteral("ui.mcp.chat"),
            QStringLiteral("Tool sync completed"),
            QJsonObject{{QStringLiteral("totalTools"), allTools.size()},
                        {QStringLiteral("mcpTools"), mcpToolCount},
                        {QStringLiteral("syncedServers"), syncedServerCount},
                        {QStringLiteral("failedServers"), failedServerCount}});
}

void McpChatWindow::renderRegisteredServers()
{
    m_registeredServersView->clear();
    if (!m_serverManager) {
        logWarn(QStringLiteral("ui.mcp.chat"), QStringLiteral("Cannot render registered servers because manager is missing"));
        return;
    }

    const QVector<qtllm::tools::mcp::McpServerDefinition> servers = m_serverManager->allServers();
    for (const qtllm::tools::mcp::McpServerDefinition &server : servers) {
        const QString state = server.enabled ? QStringLiteral("enabled") : QStringLiteral("disabled");
        m_registeredServersView->addItem(QStringLiteral("%1 [%2, %3]")
                                             .arg(server.serverId,
                                                  server.transport,
                                                  state));
    }
}

void McpChatWindow::renderAllTools()
{
    if (!m_toolRegistry) {
        m_allToolsView->clear();
        return;
    }

    QStringList lines;
    const QList<qtllm::tools::LlmToolDefinition> tools = m_toolRegistry->allTools();
    for (const qtllm::tools::LlmToolDefinition &tool : tools) {
        lines.append(QStringLiteral("- %1 [%2]").arg(tool.toolId, tool.category));
        lines.append(QStringLiteral("  invocation: ") + tool.invocationName);
        lines.append(QStringLiteral("  name: ") + tool.name);
        lines.append(QStringLiteral("  desc: ") + tool.description);
        if (!tool.capabilityTags.isEmpty()) {
            lines.append(QStringLiteral("  tags: ") + tool.capabilityTags.join(QStringLiteral(", ")));
        }
    }

    if (lines.isEmpty()) {
        lines.append(QStringLiteral("Tool registry is empty."));
    }

    m_allToolsView->setPlainText(lines.join(QStringLiteral("\n")));
}

void McpChatWindow::appendChatLine(const QString &role, const QString &text)
{
    m_chatOutput->append(QStringLiteral("[%1]\n%2\n")
                             .arg(role, text.trimmed()));
}

void McpChatWindow::appendRuntimeLog(const QString &line)
{
    m_runtimeLog->append(line);
}

void McpChatWindow::logInfo(const QString &category, const QString &message, const QJsonObject &fields) const
{
    qtllm::logging::LogContext context;
    if (m_conversationClient) {
        context.clientId = m_conversationClient->uid();
        context.sessionId = m_conversationClient->activeSessionId();
    }
    qtllm::logging::QtLlmLogger::instance().info(category, message, context, fields);
}

void McpChatWindow::logWarn(const QString &category, const QString &message, const QJsonObject &fields) const
{
    qtllm::logging::LogContext context;
    if (m_conversationClient) {
        context.clientId = m_conversationClient->uid();
        context.sessionId = m_conversationClient->activeSessionId();
    }
    qtllm::logging::QtLlmLogger::instance().warn(category, message, context, fields);
}

void McpChatWindow::logError(const QString &category, const QString &message, const QJsonObject &fields) const
{
    qtllm::logging::LogContext context;
    if (m_conversationClient) {
        context.clientId = m_conversationClient->uid();
        context.sessionId = m_conversationClient->activeSessionId();
    }
    qtllm::logging::QtLlmLogger::instance().error(category, message, context, fields);
}












void McpChatWindow::onProviderPayloadPrepared(const QString &url, const QString &payloadJson)
{
    QString body = payloadJson;
    if (!url.trimmed().isEmpty()) {
        body.prepend(QStringLiteral("URL: ") + url + QStringLiteral("\n\n"));
    }
    m_requestPayloadView->setPlainText(body);
}



