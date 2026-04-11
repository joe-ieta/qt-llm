#include "toolmergeservice.h"

#include "../identity/compactid.h"
#include "toolcatalogservice.h"
#include "toolworkspaceservice.h"

#include <QHash>

namespace qtllm::toolsstudio {

namespace {

QString makeNodeId()
{
    return qtllm::identity::generateId(qtllm::identity::IdKind::Node);
}

QString makePlacementId()
{
    return qtllm::identity::generateId(qtllm::identity::IdKind::Placement);
}

bool editableFieldsDiffer(const qtllm::tools::LlmToolDefinition &a, const qtllm::tools::LlmToolDefinition &b)
{
    return a.name != b.name
        || a.description != b.description
        || a.inputSchema != b.inputSchema
        || a.capabilityTags != b.capabilityTags
        || a.enabled != b.enabled;
}

} // namespace

ToolMergePreview ToolMergeService::buildPreview(const ToolImportPackage &package,
                                                const QList<qtllm::tools::LlmToolDefinition> &localTools,
                                                const ToolWorkspaceSnapshot &localWorkspace) const
{
    ToolMergePreview preview;
    preview.mergedWorkspace = localWorkspace;

    QHash<QString, qtllm::tools::LlmToolDefinition> localById;
    for (const qtllm::tools::LlmToolDefinition &tool : localTools) {
        localById.insert(tool.toolId, tool);
    }

    QSet<QString> importableToolIds;
    for (const qtllm::tools::LlmToolDefinition &tool : package.toolSnapshots) {
        importableToolIds.insert(tool.toolId);
        if (!localById.contains(tool.toolId)) {
            preview.toolsToInsert.append(tool);
            ToolMergeDecision decision;
            decision.toolId = tool.toolId;
            decision.action = QStringLiteral("insert_tool");
            decision.reason = QStringLiteral("Tool snapshot is missing locally.");
            preview.decisions.append(decision);
            continue;
        }

        if (editableFieldsDiffer(localById.value(tool.toolId), tool)) {
            ToolMergeIssue issue;
            issue.issueType = QStringLiteral("tool_snapshot_conflict");
            issue.severity = QStringLiteral("warning");
            issue.toolId = tool.toolId;
            issue.message = QStringLiteral("Local tool differs from imported snapshot; local version will be kept.");
            preview.issues.append(issue);

            ToolMergeDecision decision;
            decision.toolId = tool.toolId;
            decision.action = QStringLiteral("keep_local_tool");
            decision.reason = QStringLiteral("Existing local tool takes precedence over imported editable fields.");
            preview.decisions.append(decision);
        }
    }

    QSet<QString> missingRefs;
    for (const QString &toolId : package.toolRefs) {
        if (!localById.contains(toolId) && !importableToolIds.contains(toolId)) {
            missingRefs.insert(toolId);
            ToolMergeIssue issue;
            issue.issueType = QStringLiteral("missing_local_tool");
            issue.severity = QStringLiteral("warning");
            issue.toolId = toolId;
            issue.message = QStringLiteral("Imported workspace references a tool that is missing locally and not bundled in toolSnapshots.");
            preview.issues.append(issue);
        }
    }

    QHash<QString, QString> nodeIdMap;
    const QString importRootId = makeNodeId();
    const bool preserveImportedRoot = package.manifest.value(QStringLiteral("exportMode")).toString()
        == QStringLiteral("subtree");
    ToolCategoryNode importRoot;
    importRoot.nodeId = importRootId;
    importRoot.parentNodeId = localWorkspace.rootNodeId;
    importRoot.name = package.workspace.name.isEmpty() ? QStringLiteral("Imported Workspace") : QStringLiteral("Imported: ") + package.workspace.name;
    importRoot.description = package.workspace.description;
    importRoot.order = preview.mergedWorkspace.nodes.size() * 10;
    preview.mergedWorkspace.nodes.append(importRoot);

    for (const ToolCategoryNode &node : package.workspace.nodes) {
        if (node.nodeId == package.workspace.rootNodeId) {
            if (!preserveImportedRoot) {
                continue;
            }
        }

        ToolCategoryNode mapped = node;
        mapped.nodeId = makeNodeId();
        if (node.nodeId == package.workspace.rootNodeId || node.parentNodeId == package.workspace.rootNodeId) {
            mapped.parentNodeId = importRootId;
        }
        nodeIdMap.insert(node.nodeId, mapped.nodeId);
        preview.mergedWorkspace.nodes.append(mapped);
    }

    for (ToolCategoryNode &node : preview.mergedWorkspace.nodes) {
        if (node.nodeId == importRootId) {
            continue;
        }
        if (nodeIdMap.contains(node.parentNodeId)) {
            node.parentNodeId = nodeIdMap.value(node.parentNodeId);
        }
    }

    int importedPlacementCount = 0;
    int skippedPlacementCount = 0;
    for (const ToolPlacement &placement : package.workspace.placements) {
        if (missingRefs.contains(placement.toolId)) {
            ++skippedPlacementCount;
            ToolMergeIssue issue;
            issue.issueType = QStringLiteral("skipped_missing_tool_reference");
            issue.severity = QStringLiteral("warning");
            issue.toolId = placement.toolId;
            issue.message = QStringLiteral("Placement was skipped because its tool is unresolved locally.");
            preview.issues.append(issue);
            continue;
        }

        ToolPlacement mapped = placement;
        mapped.placementId = makePlacementId();
        if (placement.nodeId == package.workspace.rootNodeId) {
            mapped.nodeId = importRootId;
        } else if (nodeIdMap.contains(placement.nodeId)) {
            mapped.nodeId = nodeIdMap.value(placement.nodeId);
        } else {
            ++skippedPlacementCount;
            continue;
        }
        preview.mergedWorkspace.placements.append(mapped);
        ++importedPlacementCount;
    }

    preview.summary = QStringLiteral("Tools to insert: %1, issues: %2, imported placements: %3, skipped placements: %4")
        .arg(preview.toolsToInsert.size())
        .arg(preview.issues.size())
        .arg(importedPlacementCount)
        .arg(skippedPlacementCount);

    return preview;
}

bool ToolMergeService::applyPreview(const ToolMergePreview &preview,
                                    ToolCatalogService *catalogService,
                                    ToolWorkspaceService *workspaceService,
                                    QString *errorMessage) const
{
    if (!catalogService || !workspaceService) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Merge apply requires both catalog and workspace services");
        }
        return false;
    }

    for (const qtllm::tools::LlmToolDefinition &tool : preview.toolsToInsert) {
        if (!catalogService->registry()->registerTool(tool)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("Failed to register imported tool: ") + tool.toolId;
            }
            return false;
        }
    }
    if (!catalogService->save(errorMessage)) {
        return false;
    }

    if (!workspaceService->replaceWorkspace(preview.mergedWorkspace, errorMessage)) {
        return false;
    }

    return true;
}

} // namespace qtllm::toolsstudio
