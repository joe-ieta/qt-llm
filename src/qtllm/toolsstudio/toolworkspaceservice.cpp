#include "toolworkspaceservice.h"

#include "../identity/compactid.h"
#include "toolworkspacerepository.h"

#include <QDateTime>

#include <algorithm>

namespace qtllm::toolsstudio {

ToolWorkspaceService::ToolWorkspaceService(std::shared_ptr<ToolWorkspaceRepository> repository)
    : m_repository(std::move(repository))
{
}

bool ToolWorkspaceService::initialize(QString preferredWorkspaceId, QString *errorMessage)
{
    ToolWorkspaceIndex index = loadOrCreateIndex(errorMessage);
    QString workspaceId = preferredWorkspaceId.trimmed();
    if (workspaceId.isEmpty()) {
        workspaceId = index.lastWorkspaceId.trimmed();
    }
    if (workspaceId.isEmpty()) {
        workspaceId = QStringLiteral("default");
    }

    const std::optional<ToolWorkspaceSnapshot> snapshot = m_repository->loadWorkspace(workspaceId, nullptr);
    if (snapshot.has_value()) {
        m_currentWorkspace = *snapshot;
    } else {
        m_currentWorkspace = defaultWorkspace(workspaceId, QStringLiteral("Default Workspace"));
        if (!m_repository->saveWorkspace(m_currentWorkspace, errorMessage)) {
            return false;
        }
    }

    return saveIndex(m_currentWorkspace.workspaceId, errorMessage);
}

bool ToolWorkspaceService::loadWorkspace(const QString &workspaceId, QString *errorMessage)
{
    const std::optional<ToolWorkspaceSnapshot> snapshot = m_repository->loadWorkspace(workspaceId, errorMessage);
    if (!snapshot.has_value()) {
        return false;
    }

    m_currentWorkspace = *snapshot;
    return saveIndex(m_currentWorkspace.workspaceId, errorMessage);
}

bool ToolWorkspaceService::saveWorkspace(QString *errorMessage)
{
    m_currentWorkspace.updatedAt = QDateTime::currentDateTimeUtc();
    if (!m_repository->saveWorkspace(m_currentWorkspace, errorMessage)) {
        return false;
    }
    return saveIndex(m_currentWorkspace.workspaceId, errorMessage);
}

bool ToolWorkspaceService::replaceWorkspace(const ToolWorkspaceSnapshot &workspace, QString *errorMessage)
{
    m_currentWorkspace = workspace;
    return saveWorkspace(errorMessage);
}

ToolWorkspaceSnapshot ToolWorkspaceService::currentWorkspace() const
{
    return m_currentWorkspace;
}

QString ToolWorkspaceService::currentWorkspaceId() const
{
    return m_currentWorkspace.workspaceId;
}

QVector<ToolWorkspaceIndexEntry> ToolWorkspaceService::workspaceEntries(QString *errorMessage) const
{
    return loadOrCreateIndex(errorMessage).workspaces;
}

QVector<ToolCategoryNode> ToolWorkspaceService::allNodes() const
{
    return m_currentWorkspace.nodes;
}

QVector<ToolPlacement> ToolWorkspaceService::allPlacements() const
{
    return m_currentWorkspace.placements;
}

QVector<ToolCategoryNode> ToolWorkspaceService::childNodes(const QString &parentNodeId) const
{
    QVector<ToolCategoryNode> out;
    for (const ToolCategoryNode &node : m_currentWorkspace.nodes) {
        if (node.parentNodeId == parentNodeId) {
            out.append(node);
        }
    }
    std::sort(out.begin(), out.end(), [](const ToolCategoryNode &a, const ToolCategoryNode &b) {
        return a.order < b.order;
    });
    return out;
}

QVector<ToolPlacement> ToolWorkspaceService::placementsInNode(const QString &nodeId, bool includeDescendants) const
{
    QSet<QString> nodeIds;
    nodeIds.insert(nodeId);
    if (includeDescendants) {
        nodeIds.unite(descendantNodeIds(nodeId));
    }

    QVector<ToolPlacement> out;
    for (const ToolPlacement &placement : m_currentWorkspace.placements) {
        if (nodeIds.contains(placement.nodeId)) {
            out.append(placement);
        }
    }
    std::sort(out.begin(), out.end(), [](const ToolPlacement &a, const ToolPlacement &b) {
        if (a.pinned != b.pinned) {
            return a.pinned;
        }
        return a.order < b.order;
    });
    return out;
}

std::optional<ToolCategoryNode> ToolWorkspaceService::findNode(const QString &nodeId) const
{
    for (const ToolCategoryNode &node : m_currentWorkspace.nodes) {
        if (node.nodeId == nodeId) {
            return node;
        }
    }
    return std::nullopt;
}

std::optional<ToolPlacement> ToolWorkspaceService::findPlacement(const QString &placementId) const
{
    for (const ToolPlacement &placement : m_currentWorkspace.placements) {
        if (placement.placementId == placementId) {
            return placement;
        }
    }
    return std::nullopt;
}

bool ToolWorkspaceService::createWorkspace(const QString &name, QString *outWorkspaceId, QString *errorMessage)
{
    const QString workspaceId = generateWorkspaceId(name);
    m_currentWorkspace = defaultWorkspace(workspaceId, name.trimmed().isEmpty() ? workspaceId : name.trimmed());
    if (!m_repository->saveWorkspace(m_currentWorkspace, errorMessage)) {
        return false;
    }
    if (outWorkspaceId) {
        *outWorkspaceId = workspaceId;
    }
    return saveIndex(workspaceId, errorMessage);
}

bool ToolWorkspaceService::removeWorkspace(const QString &workspaceId, QString *errorMessage)
{
    if (workspaceId.trimmed().isEmpty() || workspaceId == m_currentWorkspace.workspaceId) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Cannot remove the active workspace");
        }
        return false;
    }

    if (!m_repository->deleteWorkspace(workspaceId, errorMessage)) {
        return false;
    }

    ToolWorkspaceIndex index = loadOrCreateIndex(errorMessage);
    QVector<ToolWorkspaceIndexEntry> kept;
    for (const ToolWorkspaceIndexEntry &entry : index.workspaces) {
        if (entry.workspaceId != workspaceId) {
            kept.append(entry);
        }
    }
    index.workspaces = kept;

    if (index.lastWorkspaceId == workspaceId) {
        index.lastWorkspaceId = m_currentWorkspace.workspaceId;
    }

    return m_repository->saveIndex(index, errorMessage);
}

bool ToolWorkspaceService::createNode(const QString &parentNodeId,
                                      const QString &name,
                                      QString *outNodeId,
                                      QString *errorMessage)
{
    if (!nodeExists(parentNodeId)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Parent node not found");
        }
        return false;
    }

    ToolCategoryNode node;
    node.nodeId = generateNodeId();
    node.parentNodeId = parentNodeId;
    node.name = name.trimmed().isEmpty() ? QStringLiteral("New Category") : name.trimmed();
    node.order = childNodes(parentNodeId).size() * 10;
    m_currentWorkspace.nodes.append(node);
    if (outNodeId) {
        *outNodeId = node.nodeId;
    }
    return saveWorkspace(errorMessage);
}

bool ToolWorkspaceService::updateNode(const ToolCategoryNode &node, QString *errorMessage)
{
    for (ToolCategoryNode &existing : m_currentWorkspace.nodes) {
        if (existing.nodeId == node.nodeId) {
            existing.name = node.name.trimmed();
            existing.description = node.description.trimmed();
            existing.expanded = node.expanded;
            existing.color = node.color.trimmed();
            existing.iconKey = node.iconKey.trimmed();
            return saveWorkspace(errorMessage);
        }
    }

    if (errorMessage) {
        *errorMessage = QStringLiteral("Node not found");
    }
    return false;
}

bool ToolWorkspaceService::moveNode(const QString &nodeId,
                                    const QString &newParentNodeId,
                                    int newOrder,
                                    QString *errorMessage)
{
    if (nodeId == m_currentWorkspace.rootNodeId) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Root node cannot be moved");
        }
        return false;
    }

    if (!nodeExists(newParentNodeId)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Target node not found");
        }
        return false;
    }

    const QSet<QString> blockedParents = descendantNodeIds(nodeId);
    if (nodeId == newParentNodeId || blockedParents.contains(newParentNodeId)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Category cannot be moved into itself or its descendant");
        }
        return false;
    }

    for (ToolCategoryNode &node : m_currentWorkspace.nodes) {
        if (node.nodeId == nodeId) {
            node.parentNodeId = newParentNodeId;
            node.order = newOrder;
            return saveWorkspace(errorMessage);
        }
    }

    if (errorMessage) {
        *errorMessage = QStringLiteral("Node not found");
    }
    return false;
}

bool ToolWorkspaceService::reorderNodeChildren(const QString &parentNodeId,
                                               const QStringList &orderedNodeIds,
                                               QString *errorMessage)
{
    if (!nodeExists(parentNodeId)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Parent node not found");
        }
        return false;
    }

    QStringList currentIds;
    const QVector<ToolCategoryNode> children = childNodes(parentNodeId);
    for (const ToolCategoryNode &node : children) {
        currentIds.append(node.nodeId);
    }

    QStringList expectedIds = currentIds;
    QStringList providedIds = orderedNodeIds;
    expectedIds.sort(Qt::CaseSensitive);
    providedIds.sort(Qt::CaseSensitive);
    if (expectedIds != providedIds) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Ordered node list does not match current children");
        }
        return false;
    }

    for (int i = 0; i < orderedNodeIds.size(); ++i) {
        for (ToolCategoryNode &node : m_currentWorkspace.nodes) {
            if (node.nodeId == orderedNodeIds.at(i)) {
                node.parentNodeId = parentNodeId;
                node.order = i * 10;
                break;
            }
        }
    }

    return saveWorkspace(errorMessage);
}

bool ToolWorkspaceService::removeNode(const QString &nodeId, bool recursive, QString *errorMessage)
{
    if (nodeId == m_currentWorkspace.rootNodeId) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Root node cannot be removed");
        }
        return false;
    }

    const QSet<QString> descendants = descendantNodeIds(nodeId);
    if (!recursive && !descendants.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Node has child categories");
        }
        return false;
    }

    QSet<QString> removalIds = descendants;
    removalIds.insert(nodeId);

    QVector<ToolCategoryNode> keptNodes;
    for (const ToolCategoryNode &node : m_currentWorkspace.nodes) {
        if (!removalIds.contains(node.nodeId)) {
            keptNodes.append(node);
        }
    }
    m_currentWorkspace.nodes = keptNodes;

    QVector<ToolPlacement> keptPlacements;
    for (const ToolPlacement &placement : m_currentWorkspace.placements) {
        if (!removalIds.contains(placement.nodeId)) {
            keptPlacements.append(placement);
        }
    }
    m_currentWorkspace.placements = keptPlacements;

    return saveWorkspace(errorMessage);
}

bool ToolWorkspaceService::addToolToNode(const QString &nodeId,
                                         const QString &toolId,
                                         QString *outPlacementId,
                                         QString *errorMessage)
{
    if (!nodeExists(nodeId)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Node not found");
        }
        return false;
    }

    if (nodeContainsTool(nodeId, toolId.trimmed())) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Tool is already present in the target category");
        }
        return false;
    }

    ToolPlacement placement;
    placement.placementId = generatePlacementId();
    placement.nodeId = nodeId;
    placement.toolId = toolId.trimmed();
    placement.order = placementsInNode(nodeId, false).size() * 10;
    m_currentWorkspace.placements.append(placement);

    if (outPlacementId) {
        *outPlacementId = placement.placementId;
    }
    return saveWorkspace(errorMessage);
}

bool ToolWorkspaceService::updatePlacement(const ToolPlacement &placement, QString *errorMessage)
{
    for (ToolPlacement &existing : m_currentWorkspace.placements) {
        if (existing.placementId == placement.placementId) {
            existing.localTags = placement.localTags;
            existing.note = placement.note;
            existing.pinned = placement.pinned;
            return saveWorkspace(errorMessage);
        }
    }

    if (errorMessage) {
        *errorMessage = QStringLiteral("Placement not found");
    }
    return false;
}

bool ToolWorkspaceService::movePlacement(const QString &placementId,
                                         const QString &targetNodeId,
                                         int newOrder,
                                         QString *errorMessage)
{
    if (!nodeExists(targetNodeId)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Target node not found");
        }
        return false;
    }

    for (ToolPlacement &existing : m_currentWorkspace.placements) {
        if (existing.placementId == placementId) {
            if (existing.nodeId != targetNodeId && nodeContainsTool(targetNodeId, existing.toolId)) {
                if (errorMessage) {
                    *errorMessage = QStringLiteral("Tool already exists in target category");
                }
                return false;
            }

            existing.nodeId = targetNodeId;
            existing.order = newOrder;
            return saveWorkspace(errorMessage);
        }
    }

    if (errorMessage) {
        *errorMessage = QStringLiteral("Placement not found");
    }
    return false;
}

bool ToolWorkspaceService::reorderPlacements(const QString &nodeId,
                                             const QStringList &orderedPlacementIds,
                                             QString *errorMessage)
{
    if (!nodeExists(nodeId)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Target node not found");
        }
        return false;
    }

    QStringList currentIds;
    const QVector<ToolPlacement> placements = placementsInNode(nodeId, false);
    for (const ToolPlacement &placement : placements) {
        currentIds.append(placement.placementId);
    }

    QStringList expectedIds = currentIds;
    QStringList providedIds = orderedPlacementIds;
    expectedIds.sort(Qt::CaseSensitive);
    providedIds.sort(Qt::CaseSensitive);
    if (expectedIds != providedIds) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Ordered placement list does not match current node placements");
        }
        return false;
    }

    for (int i = 0; i < orderedPlacementIds.size(); ++i) {
        for (ToolPlacement &placement : m_currentWorkspace.placements) {
            if (placement.placementId == orderedPlacementIds.at(i)) {
                placement.nodeId = nodeId;
                placement.order = i * 10;
                break;
            }
        }
    }

    return saveWorkspace(errorMessage);
}

bool ToolWorkspaceService::removePlacement(const QString &placementId, QString *errorMessage)
{
    QVector<ToolPlacement> kept;
    bool removed = false;
    for (const ToolPlacement &placement : m_currentWorkspace.placements) {
        if (placement.placementId == placementId) {
            removed = true;
            continue;
        }
        kept.append(placement);
    }

    if (!removed) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Placement not found");
        }
        return false;
    }

    m_currentWorkspace.placements = kept;
    return saveWorkspace(errorMessage);
}

ToolWorkspaceSnapshot ToolWorkspaceService::defaultWorkspace(const QString &workspaceId, const QString &name) const
{
    ToolWorkspaceSnapshot snapshot;
    snapshot.workspaceId = workspaceId;
    snapshot.name = name;

    ToolCategoryNode root;
    root.nodeId = snapshot.rootNodeId;
    root.name = QStringLiteral("All Tools");
    root.expanded = true;
    snapshot.nodes.append(root);
    return snapshot;
}

bool ToolWorkspaceService::saveIndex(const QString &lastWorkspaceId, QString *errorMessage) const
{
    ToolWorkspaceIndex index = loadOrCreateIndex(errorMessage);
    index.lastWorkspaceId = lastWorkspaceId;

    if (!m_currentWorkspace.workspaceId.isEmpty()) {
        bool found = false;
        for (ToolWorkspaceIndexEntry &entry : index.workspaces) {
            if (entry.workspaceId == m_currentWorkspace.workspaceId) {
                entry.name = m_currentWorkspace.name;
                entry.fileName = m_currentWorkspace.workspaceId + QStringLiteral(".json");
                entry.updatedAt = m_currentWorkspace.updatedAt;
                found = true;
                break;
            }
        }

        if (!found) {
            ToolWorkspaceIndexEntry entry;
            entry.workspaceId = m_currentWorkspace.workspaceId;
            entry.name = m_currentWorkspace.name;
            entry.fileName = m_currentWorkspace.workspaceId + QStringLiteral(".json");
            entry.updatedAt = m_currentWorkspace.updatedAt;
            index.workspaces.append(entry);
        }
    }

    return m_repository->saveIndex(index, errorMessage);
}

ToolWorkspaceIndex ToolWorkspaceService::loadOrCreateIndex(QString *errorMessage) const
{
    const std::optional<ToolWorkspaceIndex> index = m_repository->loadIndex(errorMessage);
    if (index.has_value()) {
        return *index;
    }
    return ToolWorkspaceIndex();
}

bool ToolWorkspaceService::nodeExists(const QString &nodeId) const
{
    return findNode(nodeId).has_value();
}

bool ToolWorkspaceService::nodeContainsTool(const QString &nodeId, const QString &toolId) const
{
    for (const ToolPlacement &placement : m_currentWorkspace.placements) {
        if (placement.nodeId == nodeId && placement.toolId == toolId) {
            return true;
        }
    }
    return false;
}

QSet<QString> ToolWorkspaceService::descendantNodeIds(const QString &nodeId) const
{
    QSet<QString> out;
    QVector<QString> stack;
    stack.append(nodeId);
    while (!stack.isEmpty()) {
        const QString current = stack.takeLast();
        for (const ToolCategoryNode &node : m_currentWorkspace.nodes) {
            if (node.parentNodeId == current && !out.contains(node.nodeId)) {
                out.insert(node.nodeId);
                stack.append(node.nodeId);
            }
        }
    }
    out.remove(nodeId);
    return out;
}

QString ToolWorkspaceService::generateWorkspaceId(const QString &name) const
{
    Q_UNUSED(name)
    return identity::generateId(identity::IdKind::Workspace);
}

QString ToolWorkspaceService::generateNodeId() const
{
    return identity::generateId(identity::IdKind::Node);
}

QString ToolWorkspaceService::generatePlacementId() const
{
    return identity::generateId(identity::IdKind::Placement);
}

} // namespace qtllm::toolsstudio
