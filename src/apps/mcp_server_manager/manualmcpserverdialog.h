#pragma once

#include <tools/mcp/mcp_types.h>

#include <QDialog>

class QComboBox;
class QLineEdit;
class QTextEdit;

class ManualMcpServerDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ManualMcpServerDialog(QWidget *parent = nullptr);

    qtllm::tools::mcp::McpServerDefinition serverDefinition() const;

private slots:
    void accept() override;

private:
    static QStringList parseArgs(const QString &text);
    static QJsonObject parseJsonObjectText(const QString &text, QString *errorMessage);

private:
    qtllm::tools::mcp::McpServerDefinition m_server;
    QLineEdit *m_idEdit;
    QLineEdit *m_nameEdit;
    QComboBox *m_transportCombo;
    QLineEdit *m_commandEdit;
    QLineEdit *m_argsEdit;
    QLineEdit *m_urlEdit;
    QTextEdit *m_envEdit;
    QTextEdit *m_headersEdit;
    QLineEdit *m_timeoutEdit;
};
