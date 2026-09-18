#include "mcpservermanagerwindow.h"

#include "manualmcpserverdialog.h"
#include "mcpchatwindow.h"

#include <logging/logtypes.h>
#include <logging/qtllmlogger.h>
#include <logging/signallogsink.h>

#include <QFileDialog>
#include <QDir>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPushButton>
#include <QSplitter>
#include <QTabWidget>
#include <QTextEdit>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

namespace {

QString normalizedId(const QString &value)
{
    return value.trimmed().toLower();
}

QString formatLogEvent(const qtllm::logging::LogEvent &event)
{
    QString line = QStringLiteral("[%1][%2][%3]")
                       .arg(event.timestampUtc.toString(QStringLiteral("HH:mm:ss.zzz")),
                            qtllm::logging::logLevelToString(event.level),
                            event.category);
    line += QStringLiteral(" ") + event.message;
    if (!event.fields.isEmpty()) {
        line += QStringLiteral(" ") + QString::fromUtf8(QJsonDocument(event.fields).toJson(QJsonDocument::Compact));
    }
    return line;
}

QWidget *buildTreeTab(QTreeWidget *tree, QTextEdit *detail, QWidget *parent = nullptr)
{
    auto *splitter = new QSplitter(Qt::Vertical, parent);
    splitter->addWidget(tree);
    splitter->addWidget(detail);
    splitter->setStretchFactor(0, 2);
    splitter->setStretchFactor(1, 1);

    auto *layout = new QVBoxLayout();
    layout->addWidget(splitter);

    auto *container = new QWidget(parent);
    container->setLayout(layout);
    return container;
}

QWidget *buildToolTab(QTreeWidget *tree, QTextEdit *schemaView, QTextEdit *descriptionView, QWidget *parent = nullptr)
{
    auto *detailSplitter = new QSplitter(Qt::Horizontal, parent);
    detailSplitter->addWidget(schemaView);
    detailSplitter->addWidget(descriptionView);
    detailSplitter->setStretchFactor(0, 2);
    detailSplitter->setStretchFactor(1, 1);

    auto *mainSplitter = new QSplitter(Qt::Vertical, parent);
    mainSplitter->addWidget(tree);
    mainSplitter->addWidget(detailSplitter);
    mainSplitter->setStretchFactor(0, 2);
    mainSplitter->setStretchFactor(1, 1);

    auto *layout = new QVBoxLayout();
    layout->addWidget(mainSplitter);

    auto *container = new QWidget(parent);
    container->setLayout(layout);
    return container;
}

QString prettyJson(const QJsonObject &object)
{
    if (object.isEmpty()) {
        return QStringLiteral("{}");
    }
    return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Indented));
}

} // namespace

McpServerManagerWindow::McpServerManagerWindow(QWidget *parent)
    : QWidget(parent)
    , m_serverManager(std::make_shared<qtllm::tools::mcp::McpServerManager>())
    , m_mcpClient(std::make_shared<qtllm::tools::mcp::DefaultMcpClient>())
    , m_logSink(std::make_shared<qtllm::logging::SignalLogSink>())
    , m_discoveryService(std::make_shared<McpServerDiscoveryService>(m_mcpClient))
    , m_scanLocationsList(new QListWidget(this))
    , m_addScanDirectoryButton(new QPushButton(QStringLiteral("Add Folder"), this))
    , m_removeScanDirectoryButton(new QPushButton(QStringLiteral("Remove Folder"), this))
    , m_resetScanLocationsButton(new QPushButton(QStringLiteral("Reset Built-ins"), this))
    , m_scanButton(new QPushButton(QStringLiteral("Scan Now"), this))
    , m_candidateList(new QListWidget(this))
    , m_registerButton(new QPushButton(QStringLiteral("Register Available"), this))
    , m_addManualButton(new QPushButton(QStringLiteral("Add Server..."), this))
    , m_registeredList(new QListWidget(this))
    , m_removeButton(new QPushButton(QStringLiteral("Remove Registered"), this))
    , m_openChatButton(new QPushButton(QStringLiteral("MCP Server Test"), this))
    , m_detailTitle(new QLabel(QStringLiteral("No server selected"), this))
    , m_detailMeta(new QTextEdit(this))
    , m_toolsTree(new QTreeWidget(this))
    , m_toolSchemaView(new QTextEdit(this))
    , m_toolDescriptionView(new QTextEdit(this))
    , m_resourcesTree(new QTreeWidget(this))
    , m_resourceDetailView(new QTextEdit(this))
    , m_promptsTree(new QTreeWidget(this))
    , m_promptDetailView(new QTextEdit(this))
    , m_logView(new QTextEdit(this))
    , m_refreshDetailsButton(new QPushButton(QStringLiteral("Refresh Details"), this))
    , m_capabilityTabs(new QTabWidget(this))
    , m_mainTabs(new QTabWidget(this))
{
    setWindowTitle(QStringLiteral("MCP Server Manager"));
    resize(1560, 940);

    qtllm::logging::QtLlmLogger::instance().addSink(m_logSink);
    connect(m_logSink.get(), &qtllm::logging::SignalLogSink::logEventReceived,
            this, &McpServerManagerWindow::onLogEventReceived);

    m_scanLocationsList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_detailMeta->setReadOnly(true);
    m_toolSchemaView->setReadOnly(true);
    m_toolDescriptionView->setReadOnly(true);
    m_resourceDetailView->setReadOnly(true);
    m_promptDetailView->setReadOnly(true);
    m_logView->setReadOnly(true);
    m_toolSchemaView->setPlaceholderText(QStringLiteral("Tool schema"));
    m_toolDescriptionView->setPlaceholderText(QStringLiteral("Tool description"));

    m_toolsTree->setColumnCount(3);
    m_toolsTree->setHeaderLabels(QStringList({QStringLiteral("Tool"),
                                              QStringLiteral("Description"),
                                              QStringLiteral("Schema")}));
    m_resourcesTree->setColumnCount(3);
    m_resourcesTree->setHeaderLabels(QStringList({QStringLiteral("Resource"),
                                                  QStringLiteral("URI"),
                                                  QStringLiteral("Description")}));
    m_promptsTree->setColumnCount(3);
    m_promptsTree->setHeaderLabels(QStringList({QStringLiteral("Prompt"),
                                                QStringLiteral("Description"),
                                                QStringLiteral("Args")}));

    auto *scanButtons = new QHBoxLayout();
    scanButtons->addWidget(m_addScanDirectoryButton);
    scanButtons->addWidget(m_removeScanDirectoryButton);
    scanButtons->addWidget(m_resetScanLocationsButton);
    scanButtons->addStretch(1);
    scanButtons->addWidget(m_scanButton);

    auto *scanLayout = new QVBoxLayout();
    scanLayout->addWidget(new QLabel(QStringLiteral("Scan Locations"), this));
    scanLayout->addWidget(m_scanLocationsList, 1);
    scanLayout->addLayout(scanButtons);
    auto *scanPane = new QWidget(this);
    scanPane->setLayout(scanLayout);

    auto *candidateButtons = new QHBoxLayout();
    candidateButtons->addWidget(m_addManualButton);
    candidateButtons->addWidget(m_registerButton);

    auto *candidateLayout = new QVBoxLayout();
    candidateLayout->addWidget(new QLabel(QStringLiteral("Discovered / Unregistered Servers"), this));
    candidateLayout->addWidget(m_candidateList, 1);
    candidateLayout->addLayout(candidateButtons);
    auto *candidatePane = new QWidget(this);
    candidatePane->setLayout(candidateLayout);

    auto *registeredButtons = new QHBoxLayout();
    registeredButtons->addWidget(m_removeButton);
    registeredButtons->addWidget(m_openChatButton);

    auto *registeredLayout = new QVBoxLayout();
    registeredLayout->addWidget(new QLabel(QStringLiteral("Registered Servers"), this));
    registeredLayout->addWidget(m_registeredList, 1);
    registeredLayout->addLayout(registeredButtons);
    auto *registeredPane = new QWidget(this);
    registeredPane->setLayout(registeredLayout);

    auto *leftSplitter = new QSplitter(Qt::Vertical, this);
    leftSplitter->addWidget(scanPane);
    leftSplitter->addWidget(candidatePane);
    leftSplitter->addWidget(registeredPane);
    leftSplitter->setStretchFactor(0, 1);
    leftSplitter->setStretchFactor(1, 2);
    leftSplitter->setStretchFactor(2, 2);

    m_capabilityTabs->addTab(buildToolTab(m_toolsTree, m_toolSchemaView, m_toolDescriptionView, this), QStringLiteral("Tools"));
    m_capabilityTabs->addTab(buildTreeTab(m_resourcesTree, m_resourceDetailView, this), QStringLiteral("Resources"));
    m_capabilityTabs->addTab(buildTreeTab(m_promptsTree, m_promptDetailView, this), QStringLiteral("Prompts"));

    auto *toolsLayout = new QVBoxLayout();
    toolsLayout->addWidget(m_detailTitle);
    toolsLayout->addWidget(m_detailMeta, 1);
    toolsLayout->addWidget(m_refreshDetailsButton);
    toolsLayout->addWidget(m_capabilityTabs, 3);
    auto *toolsPane = new QWidget(this);
    toolsPane->setLayout(toolsLayout);

    m_mainTabs->addTab(toolsPane, QStringLiteral("Selected Server"));
    m_mainTabs->addTab(m_logView, QStringLiteral("Logs"));

    auto *mainSplitter = new QSplitter(Qt::Horizontal, this);
    mainSplitter->addWidget(leftSplitter);
    mainSplitter->addWidget(m_mainTabs);
    mainSplitter->setStretchFactor(0, 2);
    mainSplitter->setStretchFactor(1, 3);

    auto *rootLayout = new QHBoxLayout(this);
    rootLayout->addWidget(mainSplitter);
    setLayout(rootLayout);

    connect(m_scanButton, &QPushButton::clicked, this, &McpServerManagerWindow::onScanClicked);
    connect(m_addScanDirectoryButton, &QPushButton::clicked, this, &McpServerManagerWindow::onAddScanDirectoryClicked);
    connect(m_removeScanDirectoryButton, &QPushButton::clicked, this, &McpServerManagerWindow::onRemoveSelectedScanDirectoryClicked);
    connect(m_resetScanLocationsButton, &QPushButton::clicked, this, &McpServerManagerWindow::onResetScanLocationsClicked);
    connect(m_registerButton, &QPushButton::clicked, this, &McpServerManagerWindow::onRegisterSelectedClicked);
    connect(m_addManualButton, &QPushButton::clicked, this, &McpServerManagerWindow::onAddManualClicked);
    connect(m_removeButton, &QPushButton::clicked, this, &McpServerManagerWindow::onRemoveRegisteredClicked);
    connect(m_candidateList, &QListWidget::itemSelectionChanged, this, &McpServerManagerWindow::onCandidateSelectionChanged);
    connect(m_registeredList, &QListWidget::itemSelectionChanged, this, &McpServerManagerWindow::onRegisteredSelectionChanged);
    connect(m_refreshDetailsButton, &QPushButton::clicked, this, &McpServerManagerWindow::onRefreshDetailsClicked);
    connect(m_openChatButton, &QPushButton::clicked, this, &McpServerManagerWindow::onOpenChatClicked);
    connect(m_toolsTree, &QTreeWidget::itemSelectionChanged, this, &McpServerManagerWindow::onToolSelectionChanged);
    connect(m_resourcesTree, &QTreeWidget::itemSelectionChanged, this, &McpServerManagerWindow::onResourceSelectionChanged);
    connect(m_promptsTree, &QTreeWidget::itemSelectionChanged, this, &McpServerManagerWindow::onPromptSelectionChanged);

    loadScanLocations();
    loadRegisteredServers();
    onScanClicked();
}

McpServerManagerWindow::~McpServerManagerWindow()
{
    qtllm::logging::QtLlmLogger::instance().removeSink(m_logSink);
}

void McpServerManagerWindow::onScanClicked()
{
    rebuildDiscoveryEntries();
    appendLog(QStringLiteral("Scan completed. %1 discovered, %2 registered.")
                  .arg(m_candidateList->count())
                  .arg(m_registeredList->count()));
}

void McpServerManagerWindow::onRegisterSelectedClicked()
{
    QListWidgetItem *item = m_candidateList->currentItem();
    if (!item) {
        appendLog(QStringLiteral("No discovered server selected."));
        return;
    }

    const QString id = item->data(Qt::UserRole).toString();
    const auto it = m_discoveryById.constFind(id);
    if (it == m_discoveryById.constEnd()) {
        appendLog(QStringLiteral("Selected discovered server no longer exists."));
        return;
    }

    if (!McpServerDiscoveryService::isRegistrable(it.value())) {
        appendLog(QStringLiteral("Registration blocked. Only available MCP servers can be registered."));
        return;
    }

    QString errorMessage;
    if (!m_serverManager->registerServer(it.value().server, &errorMessage)) {
        appendLog(QStringLiteral("Register failed: ") + errorMessage);
        return;
    }

    appendLog(QStringLiteral("Registered MCP server: ") + id);
    loadRegisteredServers();
    rebuildDiscoveryEntries();
    if (m_chatWindow) {
        m_chatWindow->reloadRegisteredServers();
    }
}

void McpServerManagerWindow::onAddManualClicked()
{
    ManualMcpServerDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const qtllm::tools::mcp::McpServerDefinition server = dialog.serverDefinition();
    const QString id = normalizedId(server.serverId);
    if (id.isEmpty()) {
        return;
    }

    m_manualById.insert(id, server);
    appendLog(QStringLiteral("Added manual MCP server candidate: ") + id);
    rebuildDiscoveryEntries();

    for (int row = 0; row < m_candidateList->count(); ++row) {
        QListWidgetItem *item = m_candidateList->item(row);
        if (item->data(Qt::UserRole).toString() == id) {
            m_candidateList->setCurrentItem(item);
            break;
        }
    }
}

void McpServerManagerWindow::onRemoveRegisteredClicked()
{
    QListWidgetItem *item = m_registeredList->currentItem();
    if (!item) {
        appendLog(QStringLiteral("No registered server selected."));
        return;
    }

    const QString id = item->data(Qt::UserRole).toString();
    QString errorMessage;
    if (!m_serverManager->removeServer(id, &errorMessage)) {
        appendLog(QStringLiteral("Remove failed: ") + errorMessage);
        return;
    }

    appendLog(QStringLiteral("Removed registered MCP server: ") + id);
    loadRegisteredServers();
    rebuildDiscoveryEntries();
    if (m_chatWindow) {
        m_chatWindow->reloadRegisteredServers();
    }
}

void McpServerManagerWindow::onCandidateSelectionChanged()
{
    QListWidgetItem *item = m_candidateList->currentItem();
    if (!item) {
        return;
    }

    m_registeredList->blockSignals(true);
    m_registeredList->clearSelection();
    m_registeredList->blockSignals(false);

    const QString id = item->data(Qt::UserRole).toString();
    const auto it = m_discoveryById.constFind(id);
    if (it != m_discoveryById.constEnd()) {
        setSelectedEntry(it.value());
    }
}

void McpServerManagerWindow::onRegisteredSelectionChanged()
{
    QListWidgetItem *item = m_registeredList->currentItem();
    if (!item) {
        return;
    }

    m_candidateList->blockSignals(true);
    m_candidateList->clearSelection();
    m_candidateList->blockSignals(false);

    const QString id = item->data(Qt::UserRole).toString();
    const auto it = m_discoveryById.constFind(id);
    if (it != m_discoveryById.constEnd()) {
        setSelectedEntry(it.value());
    }
}

void McpServerManagerWindow::onRefreshDetailsClicked()
{
    refreshDetailsForSelectedEntry();
}

void McpServerManagerWindow::onOpenChatClicked()
{
    if (!m_chatWindow) {
        m_chatWindow = new McpChatWindow(m_serverManager, m_mcpClient, nullptr);
    }

    m_chatWindow->show();
    m_chatWindow->raise();
    m_chatWindow->activateWindow();
    m_chatWindow->reloadRegisteredServers();
}

void McpServerManagerWindow::onAddScanDirectoryClicked()
{
    const QString directory = QFileDialog::getExistingDirectory(this,
                                                                QStringLiteral("Select Scan Folder"),
                                                                QDir::currentPath());
    if (directory.trimmed().isEmpty()) {
        return;
    }

    const QString normalized = QDir::cleanPath(directory);
    if (!m_scanLocationConfig.customDirectories.contains(normalized, Qt::CaseInsensitive)) {
        m_scanLocationConfig.customDirectories.append(normalized);
        saveScanLocations();
        renderScanLocations();
        appendLog(QStringLiteral("Added scan folder: ") + normalized);
        onScanClicked();
    }
}

void McpServerManagerWindow::onRemoveSelectedScanDirectoryClicked()
{
    QListWidgetItem *item = m_scanLocationsList->currentItem();
    if (!item) {
        return;
    }

    const bool builtIn = item->data(Qt::UserRole + 1).toBool();
    if (builtIn) {
        appendLog(QStringLiteral("Built-in scan locations are read-only. Use Reset Built-ins to restore defaults."));
        return;
    }

    const QString directory = item->data(Qt::UserRole).toString();
    m_scanLocationConfig.customDirectories.removeAll(directory);
    saveScanLocations();
    renderScanLocations();
    appendLog(QStringLiteral("Removed scan folder: ") + directory);
    onScanClicked();
}

void McpServerManagerWindow::onResetScanLocationsClicked()
{
    const McpScanLocationConfig defaults = McpScanLocationConfig::defaultConfig();
    m_scanLocationConfig.builtInFilePatterns = defaults.builtInFilePatterns;
    saveScanLocations();
    renderScanLocations();
    appendLog(QStringLiteral("Built-in scan locations restored."));
    onScanClicked();
}

void McpServerManagerWindow::onToolSelectionChanged()
{
    QTreeWidgetItem *item = m_toolsTree->currentItem();
    if (!item) {
        m_toolSchemaView->clear();
        m_toolDescriptionView->clear();
        return;
    }

    m_toolSchemaView->setPlainText(item->data(0, Qt::UserRole).toString());
    m_toolDescriptionView->setPlainText(item->data(1, Qt::UserRole).toString());
}

void McpServerManagerWindow::onResourceSelectionChanged()
{
    QTreeWidgetItem *item = m_resourcesTree->currentItem();
    if (!item) {
        m_resourceDetailView->clear();
        return;
    }

    m_resourceDetailView->setPlainText(item->data(0, Qt::UserRole).toString());
}

void McpServerManagerWindow::onPromptSelectionChanged()
{
    QTreeWidgetItem *item = m_promptsTree->currentItem();
    if (!item) {
        m_promptDetailView->clear();
        return;
    }

    m_promptDetailView->setPlainText(item->data(0, Qt::UserRole).toString());
}

void McpServerManagerWindow::onLogEventReceived(const qtllm::logging::LogEvent &event)
{
    if (!event.category.startsWith(QStringLiteral("mcp."))
        && !event.category.startsWith(QStringLiteral("ui.mcp.chat"))) {
        return;
    }

    m_logView->append(formatLogEvent(event));
}

void McpServerManagerWindow::loadRegisteredServers()
{
    QString errorMessage;
    if (!m_serverManager->load(&errorMessage) && !errorMessage.isEmpty()) {
        appendLog(QStringLiteral("Load registered servers failed: ") + errorMessage);
    }

    m_registeredById.clear();
    const QVector<qtllm::tools::mcp::McpServerDefinition> servers = m_serverManager->allServers();
    for (const qtllm::tools::mcp::McpServerDefinition &server : servers) {
        const QString id = normalizedId(server.serverId);
        if (!id.isEmpty()) {
            m_registeredById.insert(id, server);
        }
    }
}

void McpServerManagerWindow::loadScanLocations()
{
    QString errorMessage;
    m_scanLocationConfig = m_scanLocationRepository.loadOrCreate(&errorMessage);
    if (!errorMessage.isEmpty()) {
        appendLog(QStringLiteral("Scan location config warning: ") + errorMessage);
    }
    renderScanLocations();
}

void McpServerManagerWindow::saveScanLocations()
{
    QString errorMessage;
    if (!m_scanLocationRepository.save(m_scanLocationConfig, &errorMessage) && !errorMessage.isEmpty()) {
        appendLog(QStringLiteral("Saving scan locations failed: ") + errorMessage);
    }
}

void McpServerManagerWindow::renderScanLocations()
{
    m_scanLocationsList->clear();

    for (const QString &pattern : m_scanLocationConfig.builtInFilePatterns) {
        auto *item = new QListWidgetItem(QStringLiteral("[built-in] %1").arg(pattern), m_scanLocationsList);
        item->setData(Qt::UserRole, pattern);
        item->setData(Qt::UserRole + 1, true);
    }

    for (const QString &directory : m_scanLocationConfig.customDirectories) {
        auto *item = new QListWidgetItem(QStringLiteral("[folder] %1").arg(directory), m_scanLocationsList);
        item->setData(Qt::UserRole, directory);
        item->setData(Qt::UserRole + 1, false);
    }
}

void McpServerManagerWindow::renderCandidateList()
{
    m_candidateList->clear();
    for (auto it = m_discoveryById.constBegin(); it != m_discoveryById.constEnd(); ++it) {
        if (it.value().registered) {
            continue;
        }

        auto *item = new QListWidgetItem(formatServerSummary(it.value()), m_candidateList);
        item->setData(Qt::UserRole, it.key());
        item->setToolTip(formatStatusSummary(it.value()));
    }
}

void McpServerManagerWindow::renderRegisteredList()
{
    m_registeredList->clear();
    for (auto it = m_discoveryById.constBegin(); it != m_discoveryById.constEnd(); ++it) {
        if (!it.value().registered) {
            continue;
        }

        auto *item = new QListWidgetItem(formatServerSummary(it.value()), m_registeredList);
        item->setData(Qt::UserRole, it.key());
        item->setToolTip(formatStatusSummary(it.value()));
    }
}

void McpServerManagerWindow::renderActions()
{
    const auto it = m_discoveryById.constFind(m_selectedServerId);
    const bool hasSelection = it != m_discoveryById.constEnd();
    const bool canRegister = hasSelection && McpServerDiscoveryService::isRegistrable(it.value());
    const bool isRegistered = hasSelection && it.value().registered;

    m_registerButton->setEnabled(canRegister);
    m_removeButton->setEnabled(isRegistered);
    m_refreshDetailsButton->setEnabled(hasSelection);
    m_openChatButton->setEnabled(!m_registeredById.isEmpty());
}

void McpServerManagerWindow::setSelectedEntry(const McpDiscoveredServerEntry &entry)
{
    m_selectedServerId = normalizedId(entry.server.serverId);
    m_detailTitle->setText(QStringLiteral("Selected: %1").arg(entry.server.serverId));
    renderEntryMeta(entry);
    renderEntryDetails(entry);
    renderActions();
}

void McpServerManagerWindow::clearDetails()
{
    m_selectedServerId.clear();
    m_detailTitle->setText(QStringLiteral("No server selected"));
    m_detailMeta->clear();
    m_toolsTree->clear();
    m_toolSchemaView->clear();
    m_toolDescriptionView->clear();
    m_resourcesTree->clear();
    m_resourceDetailView->clear();
    m_promptsTree->clear();
    m_promptDetailView->clear();
    renderActions();
}

void McpServerManagerWindow::renderEntryMeta(const McpDiscoveredServerEntry &entry)
{
    QStringList lines;
    lines.append(QStringLiteral("serverId: %1").arg(entry.server.serverId));
    lines.append(QStringLiteral("name: %1").arg(entry.server.name));
    lines.append(QStringLiteral("registered: %1").arg(entry.registered ? QStringLiteral("true") : QStringLiteral("false")));
    lines.append(QStringLiteral("status: %1").arg(McpServerDiscoveryService::availabilityLabel(entry.status)));
    lines.append(QStringLiteral("statusMessage: %1").arg(entry.statusMessage));
    lines.append(QStringLiteral("source: %1").arg(entry.sourceLabel));
    lines.append(QStringLiteral("transport: %1").arg(entry.server.transport));
    lines.append(QStringLiteral("enabled: %1").arg(entry.server.enabled ? QStringLiteral("true") : QStringLiteral("false")));
    lines.append(QStringLiteral("command: %1").arg(entry.server.command));
    lines.append(QStringLiteral("args: %1").arg(entry.server.args.join(QStringLiteral(", "))));
    lines.append(QStringLiteral("url: %1").arg(entry.server.url));
    lines.append(QStringLiteral("timeoutMs: %1").arg(entry.server.timeoutMs));
    lines.append(QStringLiteral("env: %1").arg(compactJson(entry.server.env)));
    lines.append(QStringLiteral("headers: %1").arg(compactJson(entry.server.headers)));
    m_detailMeta->setPlainText(lines.join(QStringLiteral("\n")));
}

void McpServerManagerWindow::renderEntryDetails(const McpDiscoveredServerEntry &entry)
{
    renderToolTree(entry.tools);
    renderResourceTree(entry.resources);
    renderPromptTree(entry.prompts);
}

void McpServerManagerWindow::refreshDetailsForSelectedEntry()
{
    const auto it = m_discoveryById.find(m_selectedServerId);
    if (it == m_discoveryById.end()) {
        appendLog(QStringLiteral("No server selected for details refresh."));
        return;
    }

    McpDiscoveredServerEntry entry = it.value();
    QString errorMessage;
    const bool ok = m_discoveryService->populateDetails(&entry, &errorMessage);
    m_discoveryById.insert(m_selectedServerId, entry);
    renderEntryMeta(entry);
    renderEntryDetails(entry);
    renderActions();

    if (ok) {
        appendLog(QStringLiteral("Details refreshed for %1").arg(entry.server.serverId));
    } else {
        appendLog(QStringLiteral("Detail refresh completed with warnings for %1: %2")
                      .arg(entry.server.serverId, errorMessage));
    }
}

void McpServerManagerWindow::renderToolTree(const QVector<qtllm::tools::mcp::McpToolDescriptor> &tools)
{
    m_toolsTree->clear();
    m_toolSchemaView->clear();
    m_toolDescriptionView->clear();

    for (const qtllm::tools::mcp::McpToolDescriptor &tool : tools) {
        auto *item = new QTreeWidgetItem(m_toolsTree);
        item->setText(0, tool.name);
        item->setText(1, tool.description);
        item->setText(2, tool.inputSchema.isEmpty() ? QStringLiteral("No schema") : QStringLiteral("JSON schema"));
        item->setData(0, Qt::UserRole, prettyJson(tool.inputSchema));
        item->setData(1, Qt::UserRole, tool.description.trimmed().isEmpty() ? QStringLiteral("(no description)") : tool.description);
    }

    if (m_toolsTree->topLevelItemCount() > 0) {
        m_toolsTree->setCurrentItem(m_toolsTree->topLevelItem(0));
    } else {
        m_toolSchemaView->setPlainText(QStringLiteral("No tools returned."));
        m_toolDescriptionView->setPlainText(QStringLiteral("No tool description available."));
    }
}

void McpServerManagerWindow::renderResourceTree(const QVector<qtllm::tools::mcp::McpResourceDescriptor> &resources)
{
    m_resourcesTree->clear();
    m_resourceDetailView->clear();

    for (const qtllm::tools::mcp::McpResourceDescriptor &resource : resources) {
        auto *item = new QTreeWidgetItem(m_resourcesTree);
        item->setText(0, resource.name);
        item->setText(1, resource.uri);
        item->setText(2, resource.description);
        item->setData(0, Qt::UserRole,
                      QStringLiteral("name: %1\nuri: %2\ndescription: %3")
                          .arg(resource.name, resource.uri, resource.description));
    }

    if (m_resourcesTree->topLevelItemCount() > 0) {
        m_resourcesTree->setCurrentItem(m_resourcesTree->topLevelItem(0));
    } else {
        m_resourceDetailView->setPlainText(QStringLiteral("No resources returned."));
    }
}

void McpServerManagerWindow::renderPromptTree(const QVector<qtllm::tools::mcp::McpPromptDescriptor> &prompts)
{
    m_promptsTree->clear();
    m_promptDetailView->clear();

    for (const qtllm::tools::mcp::McpPromptDescriptor &prompt : prompts) {
        auto *item = new QTreeWidgetItem(m_promptsTree);
        item->setText(0, prompt.name);
        item->setText(1, prompt.description);
        item->setText(2, QString::number(prompt.arguments.size()));
        item->setData(0, Qt::UserRole,
                      QStringLiteral("name: %1\ndescription: %2\narguments: %3")
                          .arg(prompt.name,
                               prompt.description,
                               QString::fromUtf8(QJsonDocument(prompt.arguments).toJson(QJsonDocument::Compact))));
    }

    if (m_promptsTree->topLevelItemCount() > 0) {
        m_promptsTree->setCurrentItem(m_promptsTree->topLevelItem(0));
    } else {
        m_promptDetailView->setPlainText(QStringLiteral("No prompts returned."));
    }
}

void McpServerManagerWindow::appendLog(const QString &line)
{
    m_logView->append(line);
    qtllm::logging::QtLlmLogger::instance().info(QStringLiteral("ui.mcp.manager"), line);
}

void McpServerManagerWindow::rebuildDiscoveryEntries()
{
    loadRegisteredServers();
    const QVector<qtllm::tools::mcp::McpServerDefinition> registeredServers = m_serverManager->allServers();
    const QVector<McpDiscoveredServerEntry> discovered = m_discoveryService->scan(m_scanLocationConfig,
                                                                                   registeredServers,
                                                                                   m_manualById);

    m_discoveryById.clear();
    for (const McpDiscoveredServerEntry &entry : discovered) {
        const QString id = normalizedId(entry.server.serverId);
        if (!id.isEmpty()) {
            m_discoveryById.insert(id, entry);
        }
    }

    renderCandidateList();
    renderRegisteredList();

    if (!m_selectedServerId.isEmpty() && m_discoveryById.contains(m_selectedServerId)) {
        setSelectedEntry(m_discoveryById.value(m_selectedServerId));
    } else {
        clearDetails();
    }
}

QString McpServerManagerWindow::formatServerSummary(const McpDiscoveredServerEntry &entry)
{
    return QStringLiteral("[%1] %2 (%3)")
        .arg(formatStatusSummary(entry),
             entry.server.serverId,
             entry.server.transport);
}

QString McpServerManagerWindow::formatStatusSummary(const McpDiscoveredServerEntry &entry)
{
    QString label = McpServerDiscoveryService::availabilityLabel(entry.status);
    if (entry.registered) {
        label.prepend(QStringLiteral("Registered / "));
    }
    return label;
}

QString McpServerManagerWindow::compactJson(const QJsonObject &object)
{
    return object.isEmpty()
        ? QStringLiteral("{}")
        : QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact));
}
