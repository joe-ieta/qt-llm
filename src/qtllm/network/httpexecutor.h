#pragma once

#include "../qtllm_global.h"

#include <QByteArray>
#include <QMetaType>
#include <QNetworkRequest>
#include <QObject>
#include <QPointer>

class QNetworkAccessManager;
class QNetworkReply;
class QTimer;

namespace qtllm {

struct HttpRequestOptions
{
    int timeoutMs = 180000;
    int maxRetries = 0;
    int retryDelayMs = 400;
};

enum class HttpErrorCategory
{
    Canceled,
    Timeout,
    Network,
    Http
};

struct HttpRequestError
{
    HttpErrorCategory category = HttpErrorCategory::Network;
    QString code;
    QString message;
    QString diagnostic;
    bool retryable = false;
    int httpStatus = 0;
    int attempt = 0;
};

class QTLLM_CORE_EXPORT HttpExecutor : public QObject
{
    Q_OBJECT
public:
    explicit HttpExecutor(QObject *parent = nullptr);

    void post(const QNetworkRequest &request, const QByteArray &payload,
              const HttpRequestOptions &options = HttpRequestOptions());
    void cancel();

signals:
    void dataReceived(const QByteArray &chunk);
    void requestFinished(const QByteArray &data);
    void requestFailed(const qtllm::HttpRequestError &error);
    void attemptStarted(int attempt);
    void attemptReset(int previousAttempt, int nextAttempt);

    // Compatibility signal. New integrations should consume requestFailed().
    void errorOccurred(const QString &message);

private:
    void startAttempt();
    void finishWithError(const HttpRequestError &error);
    void finishSuccessfully();
    HttpRequestError classifyError(QNetworkReply *reply) const;
    bool canRetry(const HttpRequestError &error) const;

private:
    QNetworkAccessManager *m_networkAccessManager;
    QPointer<QNetworkReply> m_activeReply;
    QTimer *m_timeoutTimer;
    QTimer *m_retryTimer;

    QNetworkRequest m_request;
    QByteArray m_payload;
    HttpRequestOptions m_options;

    QByteArray m_buffer;
    int m_attempt = 0;
    bool m_timedOut = false;
    bool m_cancelRequested = false;
    bool m_requestActive = false;
    bool m_terminalEmitted = false;
};

} // namespace qtllm

Q_DECLARE_METATYPE(qtllm::HttpRequestError)
