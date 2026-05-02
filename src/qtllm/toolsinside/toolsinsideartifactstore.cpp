#include "toolsinsideartifactstore.h"
#include "toolsinsidei18n.h"
#include "../identity/compactid.h"

#include <QCryptographicHash>
#include <QDir>
#include <QRegularExpression>
#include <QSaveFile>

namespace qtllm::toolsinside {

ToolsInsideArtifactStore::ToolsInsideArtifactStore(QString rootDirectory)
    : m_rootDirectory(std::move(rootDirectory))
{
}

void ToolsInsideArtifactStore::setRootDirectory(const QString &rootDirectory)
{
    m_rootDirectory = rootDirectory.trimmed().isEmpty()
        ? QStringLiteral(".qtllm/tools_inside/artifacts")
        : rootDirectory.trimmed();
}

QString ToolsInsideArtifactStore::rootDirectory() const
{
    return m_rootDirectory;
}

QString ToolsInsideArtifactStore::absolutePathForRelativePath(const QString &relativePath) const
{
    return QDir(m_rootDirectory).filePath(relativePath.trimmed());
}

QString ToolsInsideArtifactStore::traceRelativeDirectory(const QString &clientId,
                                                        const QString &sessionId,
                                                        const QString &traceId) const
{
    return sanitizePathComponent(clientId)
        + QLatin1Char('/')
        + sanitizePathComponent(sessionId)
        + QLatin1Char('/')
        + sanitizePathComponent(traceId);
}

ToolsInsideArtifactRef ToolsInsideArtifactStore::writeArtifact(const QString &clientId,
                                                               const QString &sessionId,
                                                               const QString &traceId,
                                                               const QString &kind,
                                                               const QString &mimeType,
                                                               const QByteArray &payload,
                                                               const IToolsInsideRedactionPolicy &redactionPolicy,
                                                               const QJsonObject &metadata,
                                                               const QString &preferredExtension,
                                                               QString *errorMessage) const
{
    ToolsInsideArtifactRef artifact;
    artifact.artifactId = identity::generateId(identity::IdKind::Artifact);
    artifact.clientId = clientId.trimmed();
    artifact.sessionId = sessionId.trimmed();
    artifact.traceId = traceId.trimmed();
    artifact.kind = kind.trimmed();
    artifact.mimeType = mimeType.trimmed().isEmpty() ? QStringLiteral("application/octet-stream") : mimeType.trimmed();
    artifact.metadata = metadata;

    if (artifact.clientId.isEmpty() || artifact.sessionId.isEmpty() || artifact.traceId.isEmpty() || artifact.kind.isEmpty()) {
        if (errorMessage) {
            *errorMessage = ti18n(u"Artifact identifiers are incomplete", u"\u5de5\u4ef6\u6807\u8bc6\u4fe1\u606f\u4e0d\u5b8c\u6574");
        }
        artifact.artifactId.clear();
        return artifact;
    }

    if (!ensureTraceDirectory(artifact.clientId, artifact.sessionId, artifact.traceId, errorMessage)) {
        artifact.artifactId.clear();
        return artifact;
    }

    const QByteArray persisted = redactionPolicy.redact(artifact.kind, payload, metadata);
    artifact.redactionState = persisted == payload ? QStringLiteral("raw") : QStringLiteral("redacted");
    artifact.sizeBytes = persisted.size();
    artifact.sha256 = QString::fromUtf8(QCryptographicHash::hash(persisted, QCryptographicHash::Sha256).toHex());

    const QString extension = extensionFor(artifact.mimeType, preferredExtension);
    const QString relativeDirectory = traceRelativeDirectory(artifact.clientId, artifact.sessionId, artifact.traceId);
    artifact.relativePath = relativeDirectory + QLatin1Char('/') + artifact.artifactId + extension;

    QSaveFile file(QDir(m_rootDirectory).filePath(artifact.relativePath));
    if (!file.open(QIODevice::WriteOnly)) {
        if (errorMessage) {
            *errorMessage = ti18n(u"Failed to open artifact for writing: ", u"\u6253\u5f00\u5de5\u4ef6\u5199\u5165\u5931\u8d25\uff1a") + file.errorString();
        }
        artifact.artifactId.clear();
        artifact.relativePath.clear();
        return artifact;
    }

    if (file.write(persisted) != persisted.size()) {
        if (errorMessage) {
            *errorMessage = ti18n(u"Failed to write artifact: ", u"\u5199\u5165\u5de5\u4ef6\u5931\u8d25\uff1a") + file.errorString();
        }
        artifact.artifactId.clear();
        artifact.relativePath.clear();
        return artifact;
    }

    if (!file.commit()) {
        if (errorMessage) {
            *errorMessage = ti18n(u"Failed to commit artifact: ", u"\u63d0\u4ea4\u5de5\u4ef6\u5931\u8d25\uff1a") + file.errorString();
        }
        artifact.artifactId.clear();
        artifact.relativePath.clear();
        return artifact;
    }

    return artifact;
}

QString ToolsInsideArtifactStore::extensionFor(const QString &mimeType, const QString &preferredExtension) const
{
    if (!preferredExtension.trimmed().isEmpty()) {
        const QString ext = preferredExtension.trimmed();
        return ext.startsWith(QLatin1Char('.')) ? ext : QStringLiteral(".") + ext;
    }

    if (mimeType == QStringLiteral("application/json")) {
        return QStringLiteral(".json");
    }
    if (mimeType.startsWith(QStringLiteral("text/"))) {
        return QStringLiteral(".txt");
    }
    return QStringLiteral(".bin");
}

QString ToolsInsideArtifactStore::sanitizePathComponent(const QString &value) const
{
    QString sanitized = value.trimmed();
    sanitized.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9._-]")), QStringLiteral("_"));
    return sanitized.isEmpty() ? QStringLiteral("default") : sanitized;
}

bool ToolsInsideArtifactStore::ensureTraceDirectory(const QString &clientId,
                                                    const QString &sessionId,
                                                    const QString &traceId,
                                                    QString *errorMessage) const
{
    QDir root(m_rootDirectory);
    const QString relativeDirectory = traceRelativeDirectory(clientId, sessionId, traceId);
    if (root.mkpath(relativeDirectory)) {
        return true;
    }

    if (errorMessage) {
        *errorMessage = ti18n(u"Failed to create artifact directory: ", u"\u521b\u5efa\u5de5\u4ef6\u76ee\u5f55\u5931\u8d25\uff1a") + root.filePath(relativeDirectory);
    }
    return false;
}

} // namespace qtllm::toolsinside
