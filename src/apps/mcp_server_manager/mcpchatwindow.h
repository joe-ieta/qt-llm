#pragma once

#include "../../qtllm/logging/logtypes.h"
#include "../../qtllm/tools/mcp/defaultmcpclient.h"
#include "../../qtllm/tools/mcp/mcpservermanager.h"

#include <QJsonObject>
#include <QSharedPointer>
#include <QWidget>

#include <memory>

namespace qtllm::chat {
class ConversationClient;
}

namespace qtllm::logging {
class SignalLogSink;
}

namespace qtllm::tools {
class LlmToolRegistry;
class ToolEnabledChatEntry;

namespace runtime {
class ToolExecutionLayer;
}

namespace mcp {
class McpToolSyncService;
}
}

class QComboBox;
class QLineEdit;
class QListWidget;
class QLabel;
class QPushButton;
class QTextEdit;

class McpChatWindow : public QWidget
{
    Q_OBJECT
public:
    explicit McpChatWindow(std::shared_ptr<qtllm::tools::mcp::McpServerManager> serverManager,
                           std::shared_ptr<qtllm::tools::mcp::IMcpClient> mcpClient,
                           QWidget *parent = nullptr);
    ~McpChatWindow() override;

public slots:
    void reloadRegisteredServers();

private slots:
    void onApplyConfigClicked();
    void onSyncToolsClicked();
    void onSendClicked();
    void onClearChatClicked();
    void onContentTokenReceived(const QString &token);
    void onReasoningTokenReceived(const QString &token);
    void onToolSelectionPrepared(const QStringList &toolIds);
    void onToolSchemaPrepared(const QString &schemaText);
    void onRequestPrepared(const QString &payloadJson);
    void onProviderPayloadPrepared(const QString &url, const QString &payloadJson);
    void onChatCompleted(const QString &text);
    void onChatError(const QString &message);
    void onLogEventReceived(const qtllm::logging::LogEvent &event);

private:
    bool applyChatConfig(bool logResult);
    void syncRegisteredTools();
    void renderRegisteredServers();
    void renderAllTools();
    void appendChatLine(const QString &role, const QString &text);
    void appendRuntimeLog(const QString &line);
    void refreshLocalModelList();
    bool shouldDisplayLogEvent(const qtllm::logging::LogEvent &event) const;
    void logInfo(const QString &category, const QString &message, const QJsonObject &fields = QJsonObject()) const;
    void logWarn(const QString &category, const QString &message, const QJsonObject &fields = QJsonObject()) const;
    void logError(const QString &category, const QString &message, const QJsonObject &fields = QJsonObject()) const;

private:
    std::shared_ptr<qtllm::tools::mcp::McpServerManager> m_serverManager;
    std::shared_ptr<qtllm::tools::mcp::IMcpClient> m_mcpClient;
    std::shared_ptr<qtllm::tools::LlmToolRegistry> m_toolRegistry;
    std::shared_ptr<qtllm::tools::mcp::McpToolSyncService> m_toolSyncService;
    std::shared_ptr<qtllm::tools::runtime::ToolExecutionLayer> m_executionLayer;
    std::shared_ptr<qtllm::logging::SignalLogSink> m_logSink;

    QSharedPointer<qtllm::chat::ConversationClient> m_conversationClient;
    qtllm::tools::ToolEnabledChatEntry *m_chatEntry = nullptr;

    QComboBox *m_providerCombo;
    QLineEdit *m_baseUrlEdit;
    QLineEdit *m_apiKeyEdit;
    QComboBox *m_modelCombo;
    QPushButton *m_applyConfigButton;
    QPushButton *m_syncToolsButton;
    QLabel *m_toolCountLabel;
    QListWidget *m_registeredServersView;
    QTextEdit *m_allToolsView;
    QTextEdit *m_selectedToolsView;
    QTextEdit *m_toolSchemaView;
    QTextEdit *m_requestPayloadView;
    QTextEdit *m_chatOutput;
    QTextEdit *m_reasoningOutput;
    QTextEdit *m_inputEdit;
    QPushButton *m_sendButton;
    QPushButton *m_clearChatButton;
    QComboBox *m_logLevelFilter;
    QLineEdit *m_logCategoryFilter;
    QLineEdit *m_logClientIdFilter;
    QLineEdit *m_logSessionIdFilter;
    QLineEdit *m_logRequestIdFilter;
    QTextEdit *m_runtimeLog;

    bool m_assistantContentTurnActive = false;
    bool m_reasoningTurnActive = false;
};




