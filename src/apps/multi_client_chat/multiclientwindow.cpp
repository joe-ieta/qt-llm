#include "multiclientwindow.h"

#include "../../qtllm/chat/conversationclient.h"
#include "../../qtllm/chat/conversationclientfactory.h"
#include "../../qtllm/core/llmconfig.h"
#include "../../qtllm/core/llmtypes.h"
#include "../../qtllm/logging/logtypes.h"
#include "../../qtllm/logging/qtllmlogger.h"
#include "../../qtllm/logging/signallogsink.h"
#include "../../qtllm/profile/clientprofile.h"
#include "../../qtllm/providers/illmprovider.h"
#include "../../qtllm/providers/llamacppprovider.h"
#include "../../qtllm/providers/ollamaprovider.h"
#include "../../qtllm/providers/openaiprovider.h"
#include "../../qtllm/providers/openaicompatibleprovider.h"
#include "../../qtllm/providers/vllmprovider.h"
#include "../../qtllm/runtime/managedllamacppruntime.h"
#include "../../qtllm/storage/conversationrepository.h"
#include "../../qtllm/tools/builtintools.h"
#include "../../qtllm/tools/llmtoolregistry.h"
#include "../../qtllm/tools/toolenabledchatentry.h"
#include "../../qtllm/tools/runtime/clienttoolpolicyrepository.h"
#include "../../qtllm/tools/runtime/toolcatalogrepository.h"
#include "../../qtllm/tools/runtime/toolruntime_types.h"
#include "../../qtllm/tools/mcp/mcpservermanager.h"
#include "../../qtllm/tools/mcp/mcptoolsyncservice.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPushButton>
#include <QSplitter>
#include <QTextCursor>
#include <QTabWidget>
#include <QTextEdit>
#include <QUrl>
#include <QVBoxLayout>

#include <memory>
#include <optional>

namespace {

struct ProviderOption
{
    QString id;
    QString title;
    QString defaultBaseUrl;
    bool apiKeyRequired = false;
};

const QList<ProviderOption> &providerOptions()
{
    static const QList<ProviderOption> options = {
        {QStringLiteral("llama-cpp"), QStringLiteral("llama.cpp Local"), QStringLiteral("http://127.0.0.1:18080/v1"), false},
        {QStringLiteral("ollama"), QStringLiteral("Ollama"), QStringLiteral("http://127.0.0.1:11434/v1"), false},
        {QStringLiteral("vllm"), QStringLiteral("vLLM"), QStringLiteral("http://127.0.0.1:8000/v1"), false},
        {QStringLiteral("sglang"), QStringLiteral("SGLang"), QStringLiteral("http://127.0.0.1:30000/v1"), false},
        {QStringLiteral("openai"), QStringLiteral("OpenAI"), QStringLiteral("https://api.openai.com/v1"), true},
        {QStringLiteral("openai-compatible"), QStringLiteral("OpenAI-Compatible"), QStringLiteral("http://127.0.0.1:11434/v1"), false},
    };
    return options;
}

const ProviderOption *findProviderOption(const QString &providerId)
{
    const QList<ProviderOption> &options = providerOptions();
    for (const ProviderOption &option : options) {
        if (option.id == providerId) {
            return &option;
        }
    }
    return nullptr;
}

std::unique_ptr<qtllm::ILLMProvider> createProviderById(const QString &providerId)
{
    if (qtllm::runtime::ManagedLlamaCppRuntime::isManagedProvider(providerId)) {
        return std::make_unique<qtllm::LlamaCppProvider>();
    }
    if (providerId == QStringLiteral("ollama")) {
        return std::make_unique<qtllm::OllamaProvider>();
    }
    if (providerId == QStringLiteral("vllm")) {
        return std::make_unique<qtllm::VllmProvider>();
    }
    if (providerId == QStringLiteral("openai")) {
        return std::make_unique<qtllm::OpenAIProvider>();
    }
    return std::make_unique<qtllm::OpenAICompatibleProvider>();
}

QUrl buildModelsUrl(const QString &baseUrl)
{
    QUrl url(baseUrl.trimmed());
    QString path = url.path();
    if (!path.endsWith(QStringLiteral("/models"))) {
        if (!path.endsWith('/')) {
            path += '/';
        }
        path += QStringLiteral("models");
        url.setPath(path);
    }
    return url;
}

QStringList parseTags(const QString &input)
{
    QStringList parts = input.split(',', Qt::SkipEmptyParts);
    for (QString &part : parts) {
        part = part.trimmed();
    }
    parts.removeAll(QString());
    return parts;
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
        line += QStringLiteral(" ") + QString::fromUtf8(QJsonDocument(event.fields).toJson(QJsonDocument::Compact));
    }
    return line;
}
QString joinTags(const QStringList &tags)
{
    return tags.join(QStringLiteral(", "));
}

void ensureDefaultTools(const std::shared_ptr<qtllm::tools::LlmToolRegistry> &registry, bool *changed)
{
    if (!registry) {
        return;
    }

    bool localChanged = false;
    const QList<qtllm::tools::LlmToolDefinition> merged =
        qtllm::tools::mergeWithBuiltInTools(registry->allTools(), &localChanged);
    if (localChanged) {
        registry->replaceAll(merged);
    }

    if (changed) {
        *changed = localChanged;
    }
}

} // namespace

MultiClientWindow::MultiClientWindow(QWidget *parent)
    : QWidget(parent)
    , m_repository(std::make_shared<qtllm::storage::ConversationRepository>(QStringLiteral(".qtllm/clients")))
    , m_factory(std::make_unique<qtllm::chat::ConversationClientFactory>())
    , m_toolRegistry(std::make_shared<qtllm::tools::LlmToolRegistry>())
    , m_toolCatalogRepository(std::make_shared<qtllm::tools::runtime::ToolCatalogRepository>())
    , m_clientPolicyRepository(std::make_shared<qtllm::tools::runtime::ClientToolPolicyRepository>())
    , m_mcpServerManager(std::make_shared<qtllm::tools::mcp::McpServerManager>())
    , m_mcpToolSyncService(std::make_shared<qtllm::tools::mcp::McpToolSyncService>(m_toolRegistry, m_mcpServerManager ? m_mcpServerManager->registry() : nullptr))
    , m_logSink(std::make_shared<qtllm::logging::SignalLogSink>())
    , m_toolEntry(nullptr)
    , m_clientList(new QListWidget(this))
    , m_newClientButton(new QPushButton(QStringLiteral("New Client"), this))
    , m_sessionList(new QListWidget(this))
    , m_newSessionButton(new QPushButton(QStringLiteral("New Session"), this))
    , m_centerTabs(new QTabWidget(this))
    , m_output(new QTextEdit(this))
    , m_logLevelFilter(new QComboBox(this))
    , m_logCategoryFilter(new QLineEdit(this))
    , m_logClientIdFilter(new QLineEdit(this))
    , m_logSessionIdFilter(new QLineEdit(this))
    , m_logRequestIdFilter(new QLineEdit(this))
    , m_runtimeLog(new QTextEdit(this))
    , m_input(new QTextEdit(this))
    , m_useToolsCheck(new QCheckBox(QStringLiteral("Use Tools Entry"), this))
    , m_sendButton(new QPushButton(QStringLiteral("Send"), this))
    , m_providerCombo(new QComboBox(this))
    , m_baseUrlEdit(new QLineEdit(this))
    , m_apiKeyEdit(new QLineEdit(this))
    , m_modelCombo(new QComboBox(this))
    , m_reloadModelsButton(new QPushButton(QStringLiteral("Refresh Models"), this))
    , m_applyConfigButton(new QPushButton(QStringLiteral("Apply Config"), this))
    , m_networkManager(new QNetworkAccessManager(this))
    , m_systemPromptEdit(new QTextEdit(this))
    , m_personaEdit(new QLineEdit(this))
    , m_thinkingStyleEdit(new QLineEdit(this))
    , m_skillsEdit(new QLineEdit(this))
    , m_capabilitiesEdit(new QLineEdit(this))
    , m_preferencesEdit(new QLineEdit(this))
    , m_historyWindowEdit(new QLineEdit(this))
    , m_applyProfileButton(new QPushButton(QStringLiteral("Apply Profile"), this))
{
    m_factory->setRepository(m_repository);
    bool changedCatalog = false;
    if (m_toolCatalogRepository) {
        const std::optional<qtllm::tools::runtime::ToolCatalogSnapshot> loaded =
            m_toolCatalogRepository->loadCatalog(nullptr);
        if (loaded.has_value()) {
            QList<qtllm::tools::LlmToolDefinition> loadedTools;
            for (const qtllm::tools::LlmToolDefinition &tool : loaded->tools) {
                loadedTools.append(tool);
            }
            m_toolRegistry->replaceAll(loadedTools);
        }
    }

    ensureDefaultTools(m_toolRegistry, &changedCatalog);

    if (m_mcpServerManager) {
        m_mcpServerManager->load(nullptr);
    }

    if (m_mcpToolSyncService && m_mcpServerManager) {
        const QVector<qtllm::tools::mcp::McpServerDefinition> servers = m_mcpServerManager->allServers();
        for (const qtllm::tools::mcp::McpServerDefinition &server : servers) {
            if (!server.enabled) {
                continue;
            }
            if (m_mcpToolSyncService->syncServerTools(server.serverId, nullptr)) {
                changedCatalog = true;
            }
        }
    }

    if (changedCatalog && m_toolCatalogRepository) {
        qtllm::tools::runtime::ToolCatalogSnapshot snapshot;
        const QList<qtllm::tools::LlmToolDefinition> allTools = m_toolRegistry->allTools();
        for (const qtllm::tools::LlmToolDefinition &tool : allTools) {
            snapshot.tools.append(tool);
        }
        m_toolCatalogRepository->saveCatalog(snapshot, nullptr);
    }

    m_systemPromptEdit->setPlaceholderText(QStringLiteral("System prompt"));
    m_output->setReadOnly(true);
    m_runtimeLog->setReadOnly(true);
    m_input->setPlaceholderText(QStringLiteral("Input message"));
    m_apiKeyEdit->setEchoMode(QLineEdit::Password);
    m_modelCombo->setEditable(false);
    m_logLevelFilter->addItems(QStringList({QStringLiteral("debug+"), QStringLiteral("info+"), QStringLiteral("warn+"), QStringLiteral("error")}));
    m_logCategoryFilter->setPlaceholderText(QStringLiteral("category contains..."));
    m_logClientIdFilter->setPlaceholderText(QStringLiteral("clientId contains..."));
    m_logSessionIdFilter->setPlaceholderText(QStringLiteral("sessionId contains..."));
    m_logRequestIdFilter->setPlaceholderText(QStringLiteral("requestId contains..."));

    qtllm::logging::QtLlmLogger::instance().addSink(m_logSink);
    connect(m_logSink.get(), &qtllm::logging::SignalLogSink::logEventReceived,
            this, &MultiClientWindow::onLogEventReceived);

    for (const ProviderOption &option : providerOptions()) {
        m_providerCombo->addItem(option.title, option.id);
    }

    auto *leftLayout = new QVBoxLayout();
    leftLayout->addWidget(new QLabel(QStringLiteral("Clients"), this));
    leftLayout->addWidget(m_clientList, 1);
    leftLayout->addWidget(m_newClientButton);
    leftLayout->addWidget(new QLabel(QStringLiteral("Sessions"), this));
    leftLayout->addWidget(m_sessionList, 1);
    leftLayout->addWidget(m_newSessionButton);
    auto *leftPane = new QWidget(this);
    leftPane->setLayout(leftLayout);

    auto *chatLayout = new QVBoxLayout();
    chatLayout->addWidget(m_output, 1);
    chatLayout->addWidget(m_input);
    chatLayout->addWidget(m_useToolsCheck);
    chatLayout->addWidget(m_sendButton);
    auto *chatPane = new QWidget(this);
    chatPane->setLayout(chatLayout);

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

    m_centerTabs->addTab(chatPane, QStringLiteral("Chat"));
    m_centerTabs->addTab(runtimeLogPane, QStringLiteral("Runtime Log"));
    auto *modelRowLayout = new QHBoxLayout();
    modelRowLayout->addWidget(m_modelCombo, 1);
    modelRowLayout->addWidget(m_reloadModelsButton, 0);

    auto *configForm = new QFormLayout();
    configForm->addRow(QStringLiteral("Provider"), m_providerCombo);
    configForm->addRow(QStringLiteral("Base URL"), m_baseUrlEdit);
    configForm->addRow(QStringLiteral("API Key"), m_apiKeyEdit);
    configForm->addRow(QStringLiteral("Model"), modelRowLayout);

    auto *profileForm = new QFormLayout();
    profileForm->addRow(QStringLiteral("System"), m_systemPromptEdit);
    profileForm->addRow(QStringLiteral("Persona"), m_personaEdit);
    profileForm->addRow(QStringLiteral("Thinking"), m_thinkingStyleEdit);
    profileForm->addRow(QStringLiteral("Skills"), m_skillsEdit);
    profileForm->addRow(QStringLiteral("Capabilities"), m_capabilitiesEdit);
    profileForm->addRow(QStringLiteral("Preferences"), m_preferencesEdit);
    profileForm->addRow(QStringLiteral("History Window"), m_historyWindowEdit);

    auto *rightLayout = new QVBoxLayout();
    rightLayout->addLayout(configForm);
    rightLayout->addWidget(m_applyConfigButton);
    rightLayout->addSpacing(8);
    rightLayout->addLayout(profileForm);
    rightLayout->addWidget(m_applyProfileButton);
    auto *rightPane = new QWidget(this);
    rightPane->setLayout(rightLayout);

    auto *centerLayout = new QVBoxLayout();
    centerLayout->addWidget(m_centerTabs);
    auto *centerPane = new QWidget(this);
    centerPane->setLayout(centerLayout);
    auto *splitter = new QSplitter(this);
    splitter->addWidget(leftPane);
    splitter->addWidget(centerPane);
    splitter->addWidget(rightPane);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setStretchFactor(2, 0);

    auto *rootLayout = new QHBoxLayout(this);
    rootLayout->addWidget(splitter);
    setLayout(rootLayout);

    setWindowTitle(QStringLiteral("qt-llm multi client chat"));
    resize(1280, 760);

    connect(m_newClientButton, &QPushButton::clicked, this, &MultiClientWindow::onNewClientClicked);
    connect(m_clientList, &QListWidget::itemSelectionChanged, this, &MultiClientWindow::onClientSelectionChanged);
    connect(m_newSessionButton, &QPushButton::clicked, this, &MultiClientWindow::onNewSessionClicked);
    connect(m_sessionList, &QListWidget::itemSelectionChanged, this, &MultiClientWindow::onSessionSelectionChanged);
    connect(m_sendButton, &QPushButton::clicked, this, &MultiClientWindow::onSendClicked);
    connect(m_applyProfileButton, &QPushButton::clicked, this, &MultiClientWindow::onApplyProfileClicked);
    connect(m_applyConfigButton, &QPushButton::clicked, this, &MultiClientWindow::onApplyConfigClicked);
    connect(m_reloadModelsButton, &QPushButton::clicked, this, &MultiClientWindow::onRefreshModelsClicked);
    connect(m_providerCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
        const QString providerId = m_providerCombo->currentData().toString();
        const ProviderOption *option = findProviderOption(providerId);
        if (option) {
            m_baseUrlEdit->setText(option->defaultBaseUrl);
        }
        refreshModels();
    });
    connect(m_baseUrlEdit, &QLineEdit::editingFinished, this, &MultiClientWindow::onRefreshModelsClicked);
    connect(m_apiKeyEdit, &QLineEdit::editingFinished, this, &MultiClientWindow::onRefreshModelsClicked);

    loadClients();
}

MultiClientWindow::~MultiClientWindow()
{
    qtllm::logging::QtLlmLogger::instance().removeSink(m_logSink);
}

QSharedPointer<qtllm::chat::ConversationClient> MultiClientWindow::activeClient() const
{
    const QListWidgetItem *item = m_clientList->currentItem();
    if (!item) {
        return {};
    }

    const QString clientId = item->data(Qt::UserRole).toString();
    return m_factory->find(clientId);
}

void MultiClientWindow::loadClients()
{
    const QStringList persistedIds = m_factory->persistedClientIds();
    for (const QString &clientId : persistedIds) {
        m_factory->acquire(clientId);
        addClientToList(clientId);
    }

    if (m_clientList->count() == 0) {
        onNewClientClicked();
        return;
    }

    m_clientList->setCurrentRow(0);
    onClientSelectionChanged();
}

void MultiClientWindow::addClientToList(const QString &clientId)
{
    if (clientId.isEmpty()) {
        return;
    }

    for (int i = 0; i < m_clientList->count(); ++i) {
        if (m_clientList->item(i)->data(Qt::UserRole).toString() == clientId) {
            return;
        }
    }

    auto *item = new QListWidgetItem(clientId, m_clientList);
    item->setData(Qt::UserRole, clientId);
}

void MultiClientWindow::bindActiveClient(const QSharedPointer<qtllm::chat::ConversationClient> &client)
{
    QObject::disconnect(m_tokenConn);
    QObject::disconnect(m_reasoningConn);
    QObject::disconnect(m_completedConn);
    QObject::disconnect(m_errorConn);
    QObject::disconnect(m_historyConn);
    QObject::disconnect(m_sessionsConn);
    QObject::disconnect(m_activeSessionConn);

    if (!client) {
        return;
    }

    m_tokenConn = connect(client.get(), &qtllm::chat::ConversationClient::tokenReceived, this, [this](const QString &token) {
        m_output->moveCursor(QTextCursor::End);
        if (m_reasoningVisible && !m_contentVisible) {
            m_output->insertPlainText(QStringLiteral("\n[answer] "));
        }
        m_contentVisible = true;
        m_output->insertPlainText(token);
    });

    m_reasoningConn = connect(client.get(), &qtllm::chat::ConversationClient::reasoningTokenReceived, this, [this](const QString &token) {
        m_output->moveCursor(QTextCursor::End);
        if (!m_reasoningVisible) {
            m_output->insertPlainText(QStringLiteral("\n[thinking] "));
            m_reasoningVisible = true;
        }
        m_output->insertPlainText(token);
    });

    m_completedConn = connect(client.get(), &qtllm::chat::ConversationClient::completed, this, [this](const QString &) {
        m_output->append(QString());
        m_output->append(QStringLiteral("--- done ---"));
        m_reasoningVisible = false;
        m_contentVisible = false;
    });

    m_errorConn = connect(client.get(), &qtllm::chat::ConversationClient::errorOccurred, this, [this](const QString &message) {
        m_output->append(QStringLiteral("[error] ") + message);
        m_reasoningVisible = false;
        m_contentVisible = false;
    });

    m_historyConn = connect(client.get(), &qtllm::chat::ConversationClient::historyChanged, this, [this]() {
        renderActiveHistory(activeClient());
    });

    m_sessionsConn = connect(client.get(), &qtllm::chat::ConversationClient::sessionsChanged, this, [this, client]() {
        rebuildSessionList(client);
    });

    m_activeSessionConn = connect(client.get(), &qtllm::chat::ConversationClient::activeSessionChanged, this, [this, client](const QString &) {
        rebuildSessionList(client);
        renderActiveHistory(client);
    });
}

void MultiClientWindow::rebuildSessionList(const QSharedPointer<qtllm::chat::ConversationClient> &client)
{
    m_sessionList->clear();
    if (!client) {
        return;
    }

    const QString currentSessionId = client->activeSessionId();
    int currentRow = -1;

    const QStringList ids = client->sessionIds();
    for (int i = 0; i < ids.size(); ++i) {
        const QString sessionId = ids.at(i);
        const QString title = client->sessionTitle(sessionId);
        const QString label = QStringLiteral("%1 (%2)").arg(title, sessionId.left(8));
        auto *item = new QListWidgetItem(label, m_sessionList);
        item->setData(Qt::UserRole, sessionId);
        if (sessionId == currentSessionId) {
            currentRow = i;
        }
    }

    if (currentRow >= 0) {
        m_sessionList->setCurrentRow(currentRow);
    }
}

void MultiClientWindow::renderActiveHistory(const QSharedPointer<qtllm::chat::ConversationClient> &client)
{
    m_output->clear();
    if (!client) {
        return;
    }

    const QVector<qtllm::LlmMessage> history = client->history();
    for (const qtllm::LlmMessage &message : history) {
        if (message.role == QStringLiteral("user")) {
            m_output->append(QStringLiteral("> ") + message.content);
        } else if (message.role == QStringLiteral("assistant")) {
            m_output->append(message.content);
            m_output->append(QStringLiteral("--- done ---"));
        } else {
            m_output->append(QStringLiteral("[") + message.role + QStringLiteral("] ") + message.content);
        }
    }
}

void MultiClientWindow::refreshEditorFromActiveClient(const QSharedPointer<qtllm::chat::ConversationClient> &client)
{
    if (!client) {
        return;
    }

    const qtllm::LlmConfig config = client->config();
    int providerIndex = m_providerCombo->findData(config.providerName);
    if (providerIndex < 0) {
        providerIndex = 0;
    }

    m_providerCombo->setCurrentIndex(providerIndex);

    if (config.baseUrl.isEmpty() && providerIndex >= 0) {
        m_baseUrlEdit->setText(providerOptions().at(providerIndex).defaultBaseUrl);
    } else {
        m_baseUrlEdit->setText(config.baseUrl);
    }

    m_apiKeyEdit->setText(config.apiKey);

    m_modelCombo->clear();
    if (!config.model.isEmpty()) {
        m_modelCombo->addItem(config.model, config.model);
        m_modelCombo->setCurrentIndex(0);
    }

    const qtllm::profile::ClientProfile profile = client->profile();
    m_systemPromptEdit->setPlainText(profile.systemPrompt);
    m_personaEdit->setText(profile.persona);
    m_thinkingStyleEdit->setText(profile.thinkingStyle);
    m_skillsEdit->setText(joinTags(profile.skillIds));
    m_capabilitiesEdit->setText(joinTags(profile.capabilityTags));
    m_preferencesEdit->setText(joinTags(profile.preferenceTags));
    m_historyWindowEdit->setText(QString::number(profile.memoryPolicy.maxHistoryMessages));

    refreshModels();
}

bool MultiClientWindow::applyConfigToActiveClient(bool showMessage)
{
    const QSharedPointer<qtllm::chat::ConversationClient> client = activeClient();
    if (!client) {
        return false;
    }

    const QString providerId = m_providerCombo->currentData().toString();
    const ProviderOption *option = findProviderOption(providerId);
    if (!option) {
        if (showMessage) {
            m_output->append(QStringLiteral("[config] Invalid provider"));
        }
        return false;
    }

    QString baseUrl = m_baseUrlEdit->text().trimmed();
    if (baseUrl.isEmpty()) {
        baseUrl = option->defaultBaseUrl;
        m_baseUrlEdit->setText(baseUrl);
    }

    const QUrl parsedUrl(baseUrl);
    if (!parsedUrl.isValid() || parsedUrl.scheme().isEmpty() || parsedUrl.host().isEmpty()) {
        if (showMessage) {
            m_output->append(QStringLiteral("[config] Invalid base URL"));
        }
        return false;
    }

    const QString apiKey = m_apiKeyEdit->text().trimmed();
    if (option->apiKeyRequired && apiKey.isEmpty()) {
        if (showMessage) {
            m_output->append(QStringLiteral("[config] Provider requires API key"));
        }
        return false;
    }

    const QString modelId = m_modelCombo->currentText().trimmed();
    if (modelId.isEmpty()) {
        if (showMessage) {
            m_output->append(QStringLiteral("[config] No model list available, click Refresh Models"));
        }
        return false;
    }

    qtllm::LlmConfig config = client->config();
    config.providerName = providerId;
    config.baseUrl = baseUrl;
    config.apiKey = apiKey;
    config.model = modelId;
    config.stream = true;
    if (qtllm::runtime::ManagedLlamaCppRuntime::isManagedProvider(config.providerName)) {
        config.runtimeName = QStringLiteral("llama-cpp-managed");
        const QString modelPath = m_modelCombo->currentData().toString();
        if (!modelPath.isEmpty()) {
            config.llamaCppModelPath = modelPath;
        }
    }

    client->setConfig(config);
    client->setProvider(createProviderById(providerId));
    return true;
}

void MultiClientWindow::refreshModels()
{
    const QString providerId = m_providerCombo->currentData().toString();
    const ProviderOption *option = findProviderOption(providerId);
    if (!option) {
        m_output->append(QStringLiteral("[models] Invalid provider"));
        return;
    }

    if (option->apiKeyRequired && m_apiKeyEdit->text().trimmed().isEmpty()) {
        m_modelCombo->clear();
        m_output->append(QStringLiteral("[models] This provider requires API Key before refreshing models"));
        return;
    }

    if (qtllm::runtime::ManagedLlamaCppRuntime::isManagedProvider(providerId)) {
        qtllm::runtime::LlamaCppRuntimeLayout layout;
        QString errorMessage;
        qtllm::LlmConfig config;
        config.providerName = providerId;
        const QString previousModel = m_modelCombo->currentText();
        const QString previousModelPath = m_modelCombo->currentData().toString();
        const QList<qtllm::runtime::LlamaCppLocalModel> models =
            qtllm::runtime::ManagedLlamaCppRuntime::listLocalModels(config, &layout, &errorMessage);
        if (!errorMessage.isEmpty()) {
            m_modelCombo->clear();
            m_output->append(QStringLiteral("[models] ") + errorMessage);
            return;
        }
        m_modelCombo->clear();
        for (const qtllm::runtime::LlamaCppLocalModel &model : models) {
            m_modelCombo->addItem(model.displayName, model.filePath);
        }
        if (m_modelCombo->count() == 0) {
            m_output->append(QStringLiteral("[models] No GGUF models found in ") + layout.modelsDir);
            return;
        }
        int previousIndex = previousModelPath.isEmpty() ? -1 : m_modelCombo->findData(previousModelPath);
        if (previousIndex < 0) {
            previousIndex = m_modelCombo->findText(previousModel);
        }
        m_modelCombo->setCurrentIndex(previousIndex >= 0 ? previousIndex : 0);
        m_output->append(QStringLiteral("[models] Loaded %1 local GGUF model(s) from %2")
                             .arg(m_modelCombo->count())
                             .arg(layout.modelsDir));
        return;
    }

    const QUrl modelsUrl = buildModelsUrl(m_baseUrlEdit->text());
    if (!modelsUrl.isValid() || modelsUrl.scheme().isEmpty() || modelsUrl.host().isEmpty()) {
        m_modelCombo->clear();
        m_output->append(QStringLiteral("[models] Invalid API URL, cannot fetch models"));
        return;
    }

    if (m_modelsReply) {
        m_modelsReply->deleteLater();
        m_modelsReply.clear();
    }

    const QString previousModel = m_modelCombo->currentText();
    m_modelCombo->clear();

    QNetworkRequest request(modelsUrl);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));

    const QString apiKey = m_apiKeyEdit->text().trimmed();
    if (!apiKey.isEmpty()) {
        request.setRawHeader("Authorization", QByteArray("Bearer ") + apiKey.toUtf8());
    }

    m_modelsReply = m_networkManager->get(request);
    m_modelsReply->setProperty("previousModel", previousModel);
    connect(m_modelsReply, &QNetworkReply::finished, this, &MultiClientWindow::onModelsReplyFinished);

    m_output->append(QStringLiteral("[models] Fetching model list..."));
}

void MultiClientWindow::rebuildToolEntryForActiveClient(const QSharedPointer<qtllm::chat::ConversationClient> &client)
{
    if (m_toolEntry) {
        m_toolEntry->deleteLater();
        m_toolEntry = nullptr;
    }

    if (!client || !m_toolRegistry) {
        return;
    }

    m_toolEntry = new qtllm::tools::ToolEnabledChatEntry(client, m_toolRegistry, this);
    if (m_clientPolicyRepository) {
        m_toolEntry->setClientPolicyRepository(m_clientPolicyRepository);
    }
    if (m_mcpServerManager) {
        m_toolEntry->setMcpServerRegistry(m_mcpServerManager->registry());
    }
}

void MultiClientWindow::onNewClientClicked()
{
    const QSharedPointer<qtllm::chat::ConversationClient> client = m_factory->acquire();
    addClientToList(client->uid());

    for (int i = 0; i < m_clientList->count(); ++i) {
        if (m_clientList->item(i)->data(Qt::UserRole).toString() == client->uid()) {
            m_clientList->setCurrentRow(i);
            break;
        }
    }

    onClientSelectionChanged();
}

void MultiClientWindow::onClientSelectionChanged()
{
    if (m_runtimeLog) {
        m_runtimeLog->clear();
    }

    const QSharedPointer<qtllm::chat::ConversationClient> client = activeClient();
    bindActiveClient(client);
    rebuildSessionList(client);
    renderActiveHistory(client);
    refreshEditorFromActiveClient(client);
    rebuildToolEntryForActiveClient(client);

    if (client) {
        appendRuntimeLog(QStringLiteral("[client] active=") + client->uid());
    }
}

void MultiClientWindow::onNewSessionClicked()
{
    const QSharedPointer<qtllm::chat::ConversationClient> client = activeClient();
    if (!client) {
        return;
    }

    const QString title = QStringLiteral("Session %1").arg(client->sessionIds().size() + 1);
    client->createSession(title);
}

void MultiClientWindow::onSessionSelectionChanged()
{
    const QSharedPointer<qtllm::chat::ConversationClient> client = activeClient();
    if (!client || !m_sessionList->currentItem()) {
        return;
    }

    const QString sessionId = m_sessionList->currentItem()->data(Qt::UserRole).toString();
    if (client->switchSession(sessionId)) {
        renderActiveHistory(client);
    }
}

void MultiClientWindow::onSendClicked()
{
    const QSharedPointer<qtllm::chat::ConversationClient> client = activeClient();
    if (!client) {
        return;
    }

    const QString prompt = m_input->toPlainText().trimmed();
    if (prompt.isEmpty()) {
        return;
    }

    if (!applyConfigToActiveClient(true)) {
        return;
    }

    m_reasoningVisible = false;
    m_contentVisible = false;
    m_output->append(QStringLiteral("> ") + prompt);
    m_input->clear();

    if (m_useToolsCheck->isChecked() && m_toolEntry) {
        m_toolEntry->sendUserMessage(prompt);
        return;
    }

    client->sendUserMessage(prompt);
}

void MultiClientWindow::onApplyProfileClicked()
{
    const QSharedPointer<qtllm::chat::ConversationClient> client = activeClient();
    if (!client) {
        return;
    }

    qtllm::profile::ClientProfile profile = client->profile();
    profile.systemPrompt = m_systemPromptEdit->toPlainText().trimmed();
    profile.persona = m_personaEdit->text().trimmed();
    profile.thinkingStyle = m_thinkingStyleEdit->text().trimmed();
    profile.skillIds = parseTags(m_skillsEdit->text());
    profile.capabilityTags = parseTags(m_capabilitiesEdit->text());
    profile.preferenceTags = parseTags(m_preferencesEdit->text());

    bool ok = false;
    const int maxHistory = m_historyWindowEdit->text().toInt(&ok);
    if (ok && maxHistory > 0) {
        profile.memoryPolicy.maxHistoryMessages = maxHistory;
    }

    client->setProfile(profile);
}

void MultiClientWindow::onApplyConfigClicked()
{
    applyConfigToActiveClient(true);
}

void MultiClientWindow::onRefreshModelsClicked()
{
    refreshModels();
}

void MultiClientWindow::onModelsReplyFinished()
{
    if (!m_modelsReply) {
        return;
    }

    const QByteArray payload = m_modelsReply->readAll();
    const QNetworkReply::NetworkError error = m_modelsReply->error();
    const QString errorText = m_modelsReply->errorString();
    const QString previousModel = m_modelsReply->property("previousModel").toString();

    m_modelsReply->deleteLater();
    m_modelsReply.clear();

    if (error != QNetworkReply::NoError) {
        m_output->append(QStringLiteral("[models] Failed to fetch model list: ") + errorText);
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(payload);
    if (!doc.isObject()) {
        m_output->append(QStringLiteral("[models] Model list parse failed: response is not a JSON object"));
        return;
    }

    const QJsonArray data = doc.object().value(QStringLiteral("data")).toArray();
    if (data.isEmpty()) {
        m_output->append(QStringLiteral("[models] Model list is empty; check endpoint, API key, or server status"));
        return;
    }

    m_modelCombo->clear();
    for (const QJsonValue &entry : data) {
        const QString modelId = entry.toObject().value(QStringLiteral("id")).toString();
        if (!modelId.isEmpty()) {
            m_modelCombo->addItem(modelId, modelId);
        }
    }

    if (m_modelCombo->count() == 0) {
        m_output->append(QStringLiteral("[models] Model list does not contain usable model IDs"));
        return;
    }

    const int previousIndex = m_modelCombo->findText(previousModel);
    m_modelCombo->setCurrentIndex(previousIndex >= 0 ? previousIndex : 0);
    m_output->append(QStringLiteral("[models] Model list updated"));
}

void MultiClientWindow::onLogEventReceived(const qtllm::logging::LogEvent &event)
{
    if (!shouldDisplayLogEvent(event)) {
        return;
    }

    appendRuntimeLog(formatLogEvent(event));
}

bool MultiClientWindow::shouldDisplayLogEvent(const qtllm::logging::LogEvent &event) const
{
    int minimumLevel = static_cast<int>(qtllm::logging::LogLevel::Debug);
    const QString levelFilter = m_logLevelFilter ? m_logLevelFilter->currentText().trimmed().toLower() : QStringLiteral("debug+");
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
    if (!clientIdFilter.isEmpty()) {
        if (!event.clientId.contains(clientIdFilter, Qt::CaseInsensitive)) {
            return false;
        }
    } else {
        const QSharedPointer<qtllm::chat::ConversationClient> client = activeClient();
        if (client) {
            const QString activeClientId = client->uid();
            if (!event.clientId.isEmpty() && event.clientId != activeClientId) {
                return false;
            }
        }
    }

    const QString sessionIdFilter = m_logSessionIdFilter ? m_logSessionIdFilter->text().trimmed() : QString();
    if (!sessionIdFilter.isEmpty() && !event.sessionId.contains(sessionIdFilter, Qt::CaseInsensitive)) {
        return false;
    }

    const QString requestIdFilter = m_logRequestIdFilter ? m_logRequestIdFilter->text().trimmed() : QString();
    if (!requestIdFilter.isEmpty() && !event.requestId.contains(requestIdFilter, Qt::CaseInsensitive)) {
        return false;
    }

    return event.category.startsWith(QStringLiteral("tool."))
        || event.category.startsWith(QStringLiteral("mcp."))
        || event.category.startsWith(QStringLiteral("llm."))
        || event.category.startsWith(QStringLiteral("ui.multi_client"));
}

void MultiClientWindow::appendRuntimeLog(const QString &line)
{
    if (!m_runtimeLog || line.isEmpty()) {
        return;
    }
    m_runtimeLog->append(line);
}


