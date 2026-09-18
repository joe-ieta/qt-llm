#pragma once

#include "mcpscanlocationrepository.h"
#include "mcpserverdiscoveryservice.h"

#include <tools/mcp/defaultmcpclient.h>
#include <tools/mcp/mcpservermanager.h>

#include <QHash>
#include <QJsonObject>
#include <QPointer>
#include <QWidget>

#include <memory>

namespace qtllm::logging {
struct LogEvent;
class SignalLogSink;
}

class QListWidget;
class QListWidgetItem;
class QTreeWidget;
class QTreeWidgetItem;
class QPushButton;
class QLabel;
class QTextEdit;
class QTabWidget;
class McpChatWindow;

class McpServerManagerWindow : public QWidget
{
    Q_OBJECT
public:
    explicit McpServerManagerWindow(QWidget *parent = nullptr);
    ~McpServerManagerWindow() override;

private slots:
    void onScanClicked();
    void onRegisterSelectedClicked();
    void onAddManualClicked();
    void onRemoveRegisteredClicked();
    void onCandidateSelectionChanged();
    void onRegisteredSelectionChanged();
    void onRefreshDetailsClicked();
    void onOpenChatClicked();
    void onAddScanDirectoryClicked();
    void onRemoveSelectedScanDirectoryClicked();
    void onResetScanLocationsClicked();
    void onToolSelectionChanged();
    void onResourceSelectionChanged();
    void onPromptSelectionChanged();
    void onLogEventReceived(const qtllm::logging::LogEvent &event);

private:
    void loadRegisteredServers();
    void loadScanLocations();
    void saveScanLocations();
    void renderScanLocations();
    void renderCandidateList();
    void renderRegisteredList();
    void renderActions();

    void setSelectedEntry(const McpDiscoveredServerEntry &entry);
    void clearDetails();
    void renderEntryMeta(const McpDiscoveredServerEntry &entry);
    void renderEntryDetails(const McpDiscoveredServerEntry &entry);
    void refreshDetailsForSelectedEntry();

    void renderToolTree(const QVector<qtllm::tools::mcp::McpToolDescriptor> &tools);
    void renderResourceTree(const QVector<qtllm::tools::mcp::McpResourceDescriptor> &resources);
    void renderPromptTree(const QVector<qtllm::tools::mcp::McpPromptDescriptor> &prompts);

    void appendLog(const QString &line);
    void rebuildDiscoveryEntries();

    static QString formatServerSummary(const McpDiscoveredServerEntry &entry);
    static QString formatStatusSummary(const McpDiscoveredServerEntry &entry);
    static QString compactJson(const QJsonObject &object);

private:
    std::shared_ptr<qtllm::tools::mcp::McpServerManager> m_serverManager;
    std::shared_ptr<qtllm::tools::mcp::IMcpClient> m_mcpClient;
    std::shared_ptr<qtllm::logging::SignalLogSink> m_logSink;
    std::shared_ptr<McpServerDiscoveryService> m_discoveryService;
    McpScanLocationRepository m_scanLocationRepository;
    McpScanLocationConfig m_scanLocationConfig;

    QHash<QString, McpDiscoveredServerEntry> m_discoveryById;
    QHash<QString, qtllm::tools::mcp::McpServerDefinition> m_registeredById;
    QHash<QString, qtllm::tools::mcp::McpServerDefinition> m_manualById;

    QListWidget *m_scanLocationsList;
    QPushButton *m_addScanDirectoryButton;
    QPushButton *m_removeScanDirectoryButton;
    QPushButton *m_resetScanLocationsButton;
    QPushButton *m_scanButton;

    QListWidget *m_candidateList;
    QPushButton *m_registerButton;
    QPushButton *m_addManualButton;

    QListWidget *m_registeredList;
    QPushButton *m_removeButton;
    QPushButton *m_openChatButton;

    QLabel *m_detailTitle;
    QTextEdit *m_detailMeta;
    QTreeWidget *m_toolsTree;
    QTextEdit *m_toolSchemaView;
    QTextEdit *m_toolDescriptionView;
    QTreeWidget *m_resourcesTree;
    QTextEdit *m_resourceDetailView;
    QTreeWidget *m_promptsTree;
    QTextEdit *m_promptDetailView;
    QTextEdit *m_logView;
    QPushButton *m_refreshDetailsButton;
    QTabWidget *m_capabilityTabs;
    QTabWidget *m_mainTabs;

    QString m_selectedServerId;
    QPointer<McpChatWindow> m_chatWindow;
};
