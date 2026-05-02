#pragma once

#include <QPointer>
#include <QString>
#include <QWidget>

class QComboBox;
class QLabel;
class QTextEdit;
class QLineEdit;
class QPushButton;
class QNetworkAccessManager;
class QNetworkReply;
class QObject;
class QEvent;

namespace qtllm {
namespace host {
class RuntimeFacade;
}
}

class ChatWindow : public QWidget
{
    Q_OBJECT
public:
    explicit ChatWindow(QWidget *parent = nullptr);

private slots:
    void onProviderChanged(int index);
    void onRefreshModelsClicked();
    void onModelsReplyFinished();
    void onSendClicked();

private:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void applyConfigToClient();
    bool validateConfig(bool showMessage);
    void setStatusMessage(const QString &message, bool isError);
    void refreshModels();
    QString selectedProviderId() const;

private:
    QComboBox *m_providerCombo;
    QLineEdit *m_baseUrlEdit;
    QLineEdit *m_apiKeyEdit;
    QComboBox *m_modelCombo;
    QPushButton *m_reloadModelsButton;
    QLabel *m_statusLabel;
    QTextEdit *m_output;
    QLineEdit *m_input;
    QPushButton *m_sendButton;
    qtllm::host::RuntimeFacade *m_runtime;
    QNetworkAccessManager *m_networkManager;
    QPointer<QNetworkReply> m_modelsReply;
    bool m_reasoningVisible = false;
    bool m_contentVisible = false;
};
