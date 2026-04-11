#pragma once

#include <QString>
#include <QStringView>

#include <QtGlobal>

namespace qtllm::identity {

enum class IdKind {
    Client,
    Session,
    Trace,
    Request,
    Span,
    Event,
    ToolCall,
    Artifact,
    SupportLink,
    Workspace,
    Node,
    Placement,
    Package,
    Task,
    Queue
};

QString generateId(IdKind kind);
QString generateIdWithPrefix(QStringView prefix);
QString composeId(QStringView prefix, quint64 orderValue);
QString prefixForKind(IdKind kind);
bool isValidId(QStringView value);
bool hasIdPrefix(QStringView value, QStringView prefix);
quint64 decodeIdOrder(QStringView value, bool *ok = nullptr);

} // namespace qtllm::identity
