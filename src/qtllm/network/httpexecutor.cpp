#include "httpexecutor.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>

namespace qtllm {

namespace {

constexpr int kMaxErrorBodyChars = 4096;

// Extracts a human-readable provider message from an OpenAI-compatible error
// body ({"error":{"message":...}} and close variants). Returns an empty string
// when the body carries no usable message.
QString providerErrorMessage(const QByteArray &body)
{
    const QByteArray trimmed = body.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }

    const QJsonDocument document = QJsonDocument::fromJson(trimmed);
    if (document.isObject()) {
        const QJsonObject root = document.object();
        const QJsonValue errorValue = root.value(QStringLiteral("error"));
        if (errorValue.isObject()) {
            const QString message = errorValue.toObject().value(QStringLiteral("message")).toString().trimmed();
            if (!message.isEmpty()) {
                return message;
            }
        } else if (errorValue.isString() && !errorValue.toString().trimmed().isEmpty()) {
            return errorValue.toString().trimmed();
        }

        const QString message = root.value(QStringLiteral("message")).toString().trimmed();
        if (!message.isEmpty()) {
            return message;
        }
    }

    return {};
}

} // namespace

HttpExecutor::HttpExecutor(QObject *parent)
    : QObject(parent)
    , m_networkAccessManager(new QNetworkAccessManager(this))
    , m_timeoutTimer(new QTimer(this))
    , m_retryTimer(new QTimer(this))
{
    m_timeoutTimer->setSingleShot(true);
    m_retryTimer->setSingleShot(true);

    connect(m_timeoutTimer, &QTimer::timeout, this, [this]() {
        if (!m_requestActive || !m_activeReply) {
            return;
        }

        m_timedOut = true;
        m_activeReply->abort();
    });
    connect(m_retryTimer, &QTimer::timeout, this, &HttpExecutor::startAttempt);
}

void HttpExecutor::post(const QNetworkRequest &request, const QByteArray &payload,
                        const HttpRequestOptions &options)
{
    if (m_requestActive) {
        m_timeoutTimer->stop();
        m_retryTimer->stop();
        if (m_activeReply) {
            disconnect(m_activeReply, nullptr, this, nullptr);
            m_activeReply->abort();
            m_activeReply->deleteLater();
            m_activeReply.clear();
        }

        HttpRequestError replaced;
        replaced.category = HttpErrorCategory::Canceled;
        replaced.code = QStringLiteral("request_replaced");
        replaced.message = QStringLiteral("Request replaced by a newer request");
        replaced.attempt = m_attempt;
        finishWithError(replaced);
    }

    m_request = request;
    m_payload = payload;
    m_options = options;

    if (m_options.timeoutMs <= 0) {
        m_options.timeoutMs = 60000;
    }
    if (m_options.retryDelayMs < 0) {
        m_options.retryDelayMs = 0;
    }
    if (m_options.maxRetries < 0) {
        m_options.maxRetries = 0;
    }

    m_buffer.clear();
    m_attempt = 0;
    m_timedOut = false;
    m_cancelRequested = false;
    m_requestActive = true;
    m_terminalEmitted = false;

    startAttempt();
}

void HttpExecutor::cancel()
{
    if (!m_requestActive || m_terminalEmitted) {
        return;
    }

    m_cancelRequested = true;
    m_timeoutTimer->stop();
    m_retryTimer->stop();

    if (m_activeReply) {
        m_activeReply->abort();
        return;
    }

    HttpRequestError canceled;
    canceled.category = HttpErrorCategory::Canceled;
    canceled.code = QStringLiteral("request_canceled");
    canceled.message = QStringLiteral("Request canceled");
    canceled.attempt = m_attempt;
    finishWithError(canceled);
}

void HttpExecutor::startAttempt()
{
    if (!m_requestActive || m_terminalEmitted || m_cancelRequested) {
        return;
    }

    if (m_activeReply) {
        m_activeReply->deleteLater();
        m_activeReply.clear();
    }

    m_buffer.clear();
    m_timedOut = false;
    ++m_attempt;
    emit attemptStarted(m_attempt);

    m_activeReply = m_networkAccessManager->post(m_request, m_payload);
    QNetworkReply *reply = m_activeReply.data();
    m_timeoutTimer->start(m_options.timeoutMs);

    connect(reply, &QNetworkReply::readyRead, this, [this, reply]() {
        if (!m_requestActive || m_activeReply != reply) {
            return;
        }

        const QByteArray chunk = reply->readAll();
        if (chunk.isEmpty()) {
            return;
        }

        m_buffer.append(chunk);
        emit dataReceived(chunk);
    });

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (m_activeReply != reply) {
            reply->deleteLater();
            return;
        }

        m_timeoutTimer->stop();
        const QByteArray trailingData = reply->readAll();
        if (!trailingData.isEmpty()) {
            m_buffer.append(trailingData);
            emit dataReceived(trailingData);
        }

        const HttpRequestError requestError = classifyError(reply);
        const bool failed = reply->error() != QNetworkReply::NoError || m_cancelRequested || m_timedOut;
        m_activeReply.clear();
        reply->deleteLater();

        if (failed) {
            if (!m_cancelRequested && canRetry(requestError)) {
                emit attemptReset(m_attempt, m_attempt + 1);
                m_buffer.clear();
                m_retryTimer->start(m_options.retryDelayMs);
                return;
            }

            finishWithError(requestError);
            return;
        }

        finishSuccessfully();
    });
}

void HttpExecutor::finishWithError(const HttpRequestError &error)
{
    if (!m_requestActive || m_terminalEmitted) {
        return;
    }

    m_terminalEmitted = true;
    m_requestActive = false;
    m_timeoutTimer->stop();
    m_retryTimer->stop();
    m_buffer.clear();
    emit requestFailed(error);
    emit errorOccurred(error.message);
}

void HttpExecutor::finishSuccessfully()
{
    if (!m_requestActive || m_terminalEmitted) {
        return;
    }

    m_terminalEmitted = true;
    m_requestActive = false;
    const QByteArray responseData = m_buffer;
    m_buffer.clear();
    emit requestFinished(responseData);
}

HttpRequestError HttpExecutor::classifyError(QNetworkReply *reply) const
{
    HttpRequestError error;
    error.attempt = m_attempt;
    error.diagnostic = reply ? reply->errorString() : QString();
    error.httpStatus = reply ? reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() : 0;

    if (m_cancelRequested) {
        error.category = HttpErrorCategory::Canceled;
        error.code = QStringLiteral("request_canceled");
        error.message = QStringLiteral("Request canceled");
        return error;
    }

    if (m_timedOut) {
        error.category = HttpErrorCategory::Timeout;
        error.code = QStringLiteral("request_timeout");
        error.message = QStringLiteral("Request timeout");
        error.retryable = true;
        return error;
    }

    if (error.httpStatus >= 400) {
        error.category = HttpErrorCategory::Http;
        error.code = QStringLiteral("http_error");
        error.responseBody = QString::fromUtf8(m_buffer.left(kMaxErrorBodyChars));
        const QString providerMessage = providerErrorMessage(m_buffer);
        // Prefer the provider's own error message; fall back to the Qt network
        // error string ("server replied with status code N") when the body has
        // nothing usable.
        error.message = providerMessage.isEmpty() ? error.diagnostic : providerMessage;
        error.retryable = error.httpStatus == 408 || error.httpStatus == 425
            || error.httpStatus == 429 || error.httpStatus >= 500;
        return error;
    }

    error.category = HttpErrorCategory::Network;
    error.code = QStringLiteral("network_error");
    error.message = error.diagnostic.isEmpty() ? QStringLiteral("Network request failed") : error.diagnostic;
    error.retryable = true;
    return error;
}

bool HttpExecutor::canRetry(const HttpRequestError &error) const
{
    return error.retryable && m_attempt <= m_options.maxRetries;
}

} // namespace qtllm
