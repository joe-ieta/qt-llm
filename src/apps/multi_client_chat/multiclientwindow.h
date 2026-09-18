#pragma once

#include <chat/conversationclientfactory.h>
#include <logging/logtypes.h>

#include <QMetaObject>
#include <QPointer>
#include <QSharedPointer>
#include <QWidget>

#include <memory>

namespace qtllm::chat {
class ConversationClient;
}

namespace qtllm::host {
class RuntimeFacade;
}

namespace qtllm::logging {
class SignalLogSink;
}

namespace qtllm::tools {
class LlmToolRegistry;
class ToolEnabledChatEntry;
namespace runtime {
class ToolCatalogRepository;
class ClientToolPolicyRepository;
}
namespace mcp {
class McpServerManager;
class McpToolSyncService;
}
}

namespace qtllm::storage {
class ConversationRepository;
}

class QListWidget;
class QListWidgetItem;
class QLineEdit;
class QTextEdit;
class QPushButton;
class QComboBox;
class QCheckBox;
class QTabWidget;
class QNetworkAccessManager;
class QNetworkReply;

class MultiClientWindow : public QWidget
{
    Q_OBJECT
public:
    explicit MultiClientWindow(QWidget *parent = nullptr);
    ~MultiClientWindow() override;

private slots:
    void onNewClientClicked();
    void onClientSelectionChanged();
    void onNewSessionClicked();
    void onSessionSelectionChanged();
    void onSendClicked();
    void onApplyProfileClicked();
    void onApplyConfigClicked();
    void onRefreshModelsClicked();
    void onModelsReplyFinished();
    void onLogEventReceived(const qtllm::logging::LogEvent &event);

private:
    QSharedPointer<qtllm::chat::ConversationClient> activeClient() const;
    void loadClients();
    void addClientToList(const QString &clientId);
    void bindActiveClient(const QSharedPointer<qtllm::chat::ConversationClient> &client);
    void rebuildSessionList(const QSharedPointer<qtllm::chat::ConversationClient> &client);
    void renderActiveHistory(const QSharedPointer<qtllm::chat::ConversationClient> &client);
    void refreshEditorFromActiveClient(const QSharedPointer<qtllm::chat::ConversationClient> &client);
    bool applyConfigToActiveClient(bool showMessage);
    void refreshModels();
    void rebuildToolEntryForActiveClient(const QSharedPointer<qtllm::chat::ConversationClient> &client);
    bool shouldDisplayLogEvent(const qtllm::logging::LogEvent &event) const;
    void appendRuntimeLog(const QString &line);

private:
    std::shared_ptr<qtllm::storage::ConversationRepository> m_repository;
    std::unique_ptr<qtllm::chat::ConversationClientFactory> m_factory;
    std::shared_ptr<qtllm::tools::LlmToolRegistry> m_toolRegistry;
    std::shared_ptr<qtllm::tools::runtime::ToolCatalogRepository> m_toolCatalogRepository;
    std::shared_ptr<qtllm::tools::runtime::ClientToolPolicyRepository> m_clientPolicyRepository;
    std::shared_ptr<qtllm::tools::mcp::McpServerManager> m_mcpServerManager;
    std::shared_ptr<qtllm::tools::mcp::McpToolSyncService> m_mcpToolSyncService;
    std::shared_ptr<qtllm::logging::SignalLogSink> m_logSink;
    qtllm::tools::ToolEnabledChatEntry *m_toolEntry;
    qtllm::host::RuntimeFacade *m_localRuntime;

    QListWidget *m_clientList;
    QPushButton *m_newClientButton;

    QListWidget *m_sessionList;
    QPushButton *m_newSessionButton;

    QTabWidget *m_centerTabs;
    QTextEdit *m_output;
    QComboBox *m_logLevelFilter;
    QLineEdit *m_logCategoryFilter;
    QLineEdit *m_logClientIdFilter;
    QLineEdit *m_logSessionIdFilter;
    QLineEdit *m_logRequestIdFilter;
    QTextEdit *m_runtimeLog;
    QTextEdit *m_input;
    QCheckBox *m_useToolsCheck;
    QPushButton *m_sendButton;

    QComboBox *m_providerCombo;
    QLineEdit *m_baseUrlEdit;
    QLineEdit *m_apiKeyEdit;
    QComboBox *m_modelCombo;
    QPushButton *m_reloadModelsButton;
    QPushButton *m_applyConfigButton;
    QNetworkAccessManager *m_networkManager;
    QPointer<QNetworkReply> m_modelsReply;

    QTextEdit *m_systemPromptEdit;
    QLineEdit *m_personaEdit;
    QLineEdit *m_thinkingStyleEdit;
    QLineEdit *m_skillsEdit;
    QLineEdit *m_capabilitiesEdit;
    QLineEdit *m_preferencesEdit;
    QLineEdit *m_historyWindowEdit;
    QPushButton *m_applyProfileButton;

    QMetaObject::Connection m_tokenConn;
    QMetaObject::Connection m_reasoningConn;
    QMetaObject::Connection m_completedConn;
    QMetaObject::Connection m_errorConn;
    QMetaObject::Connection m_historyConn;
    QMetaObject::Connection m_sessionsConn;
    QMetaObject::Connection m_activeSessionConn;
    bool m_reasoningVisible = false;
    bool m_contentVisible = false;
};


