#include "toolimportexportservice.h"

#include "../identity/compactid.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSaveFile>

namespace qtllm::toolsstudio {

bool ToolImportExportService::exportWorkspace(const ToolWorkspaceSnapshot &workspace,
                                              const QList<qtllm::tools::LlmToolDefinition> &catalogTools,
                                              const QString &filePath,
                                              bool includeToolSnapshots,
                                              QString *errorMessage) const
{
    return savePackage(buildPackage(workspace, catalogTools, includeToolSnapshots, QStringLiteral("workspace")),
                       filePath,
                       errorMessage);
}

bool ToolImportExportService::exportSubtree(const ToolWorkspaceSnapshot &workspace,
                                            const QString &rootNodeId,
                                            const QList<qtllm::tools::LlmToolDefinition> &catalogTools,
                                            const QString &filePath,
                                            bool includeToolSnapshots,
                                            QString *errorMessage) const
{
    const QSet<QString> nodeIds = subtreeNodeIds(workspace, rootNodeId);
    if (nodeIds.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Subtree root node was not found");
        }
        return false;
    }

    ToolWorkspaceSnapshot subtree = workspace;
    subtree.nodes.clear();
    subtree.placements.clear();
    for (const ToolCategoryNode &node : workspace.nodes) {
        if (nodeIds.contains(node.nodeId)) {
            subtree.nodes.append(node);
        }
    }
    for (const ToolPlacement &placement : workspace.placements) {
        if (nodeIds.contains(placement.nodeId)) {
            subtree.placements.append(placement);
        }
    }
    subtree.rootNodeId = rootNodeId;

    return savePackage(buildPackage(subtree, catalogTools, includeToolSnapshots, QStringLiteral("subtree")),
                       filePath,
                       errorMessage);
}

std::optional<ToolImportPackage> ToolImportExportService::loadPackage(const QString &filePath, QString *errorMessage) const
{
    QFile file(filePath);
    if (!file.exists()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Import package does not exist");
        }
        return std::nullopt;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to open package: ") + file.errorString();
        }
        return std::nullopt;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Package JSON parse error: ") + parseError.errorString();
        }
        return std::nullopt;
    }

    return ToolImportPackage::fromJson(doc.object());
}

ToolImportPackage ToolImportExportService::buildPackage(const ToolWorkspaceSnapshot &workspace,
                                                        const QList<qtllm::tools::LlmToolDefinition> &catalogTools,
                                                        bool includeToolSnapshots,
                                                        const QString &exportMode) const
{
    ToolImportPackage package;
    package.packageId = identity::generateId(identity::IdKind::Package);
    package.name = workspace.name.isEmpty() ? workspace.workspaceId : workspace.name;
    package.description = workspace.description;
    package.exportedBy.insert(QStringLiteral("app"), QStringLiteral("toolstudio"));
    package.exportedBy.insert(QStringLiteral("appVersion"), QStringLiteral("0.1.0"));
    package.workspace = workspace;

    QSet<QString> toolIds;
    for (const ToolPlacement &placement : workspace.placements) {
        toolIds.insert(placement.toolId);
    }
    package.toolRefs = QStringList(toolIds.begin(), toolIds.end());
    package.toolRefs.sort(Qt::CaseInsensitive);

    if (includeToolSnapshots) {
        for (const qtllm::tools::LlmToolDefinition &tool : catalogTools) {
            if (toolIds.contains(tool.toolId)) {
                package.toolSnapshots.append(tool);
            }
        }
    }

    package.manifest.insert(QStringLiteral("exportMode"), exportMode);
    package.manifest.insert(QStringLiteral("workspaceId"), workspace.workspaceId);
    package.manifest.insert(QStringLiteral("workspaceName"), workspace.name);
    package.manifest.insert(QStringLiteral("rootNodeId"), workspace.rootNodeId);
    package.manifest.insert(QStringLiteral("nodeCount"), workspace.nodes.size());
    package.manifest.insert(QStringLiteral("placementCount"), workspace.placements.size());
    package.manifest.insert(QStringLiteral("toolRefCount"), package.toolRefs.size());
    package.manifest.insert(QStringLiteral("toolSnapshotCount"), package.toolSnapshots.size());
    return package;
}

bool ToolImportExportService::savePackage(const ToolImportPackage &package,
                                          const QString &filePath,
                                          QString *errorMessage) const
{
    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to open export file: ") + file.errorString();
        }
        return false;
    }

    const QJsonDocument doc(package.toJson());
    if (file.write(doc.toJson(QJsonDocument::Indented)) < 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to write export file: ") + file.errorString();
        }
        return false;
    }

    if (!file.commit()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to commit export file: ") + file.errorString();
        }
        return false;
    }

    return true;
}

QSet<QString> ToolImportExportService::subtreeNodeIds(const ToolWorkspaceSnapshot &workspace, const QString &rootNodeId) const
{
    QSet<QString> out;
    bool found = false;
    for (const ToolCategoryNode &node : workspace.nodes) {
        if (node.nodeId == rootNodeId) {
            found = true;
            break;
        }
    }
    if (!found) {
        return out;
    }

    QVector<QString> stack;
    stack.append(rootNodeId);
    while (!stack.isEmpty()) {
        const QString current = stack.takeLast();
        if (out.contains(current)) {
            continue;
        }
        out.insert(current);
        for (const ToolCategoryNode &node : workspace.nodes) {
            if (node.parentNodeId == current) {
                stack.append(node.nodeId);
            }
        }
    }
    return out;
}

} // namespace qtllm::toolsstudio
