#pragma once

#include <toolsstudio/toolcatalogservice.h>
#include <toolsstudio/toolimportexportservice.h>
#include <toolsstudio/toolmergeservice.h>
#include <toolsstudio/toolstudiosettings.h>
#include <toolsstudio/toolworkspaceservice.h>

#include <QObject>
#include <memory>

class ToolStudioController : public QObject
{
    Q_OBJECT

public:
    explicit ToolStudioController(QObject *parent = nullptr);

    bool initialize(QString *errorMessage = nullptr);

    qtllm::toolsstudio::ToolCatalogService *catalogService() const;
    qtllm::toolsstudio::ToolWorkspaceService *workspaceService() const;
    qtllm::toolsstudio::ToolStudioSettings *settings() const;

    bool saveAll(QString *errorMessage = nullptr);
    bool createWorkspace(const QString &name, QString *outWorkspaceId = nullptr, QString *errorMessage = nullptr);
    bool openWorkspace(const QString &workspaceId, QString *errorMessage = nullptr);
    bool removeWorkspace(const QString &workspaceId, QString *errorMessage = nullptr);
    bool createNode(const QString &parentNodeId, const QString &name, QString *outNodeId = nullptr, QString *errorMessage = nullptr);
    bool updateNode(const qtllm::toolsstudio::ToolCategoryNode &node, QString *errorMessage = nullptr);
    bool moveNode(const QString &nodeId, const QString &newParentNodeId, int newOrder, QString *errorMessage = nullptr);
    bool reorderNodeChildren(const QString &parentNodeId, const QStringList &orderedNodeIds, QString *errorMessage = nullptr);
    bool removeNode(const QString &nodeId, bool recursive, QString *errorMessage = nullptr);
    bool addToolToNode(const QString &nodeId, const QString &toolId, QString *outPlacementId = nullptr, QString *errorMessage = nullptr);
    bool updatePlacement(const qtllm::toolsstudio::ToolPlacement &placement, QString *errorMessage = nullptr);
    bool movePlacement(const QString &placementId, const QString &targetNodeId, int newOrder, QString *errorMessage = nullptr);
    bool reorderPlacements(const QString &nodeId, const QStringList &orderedPlacementIds, QString *errorMessage = nullptr);
    bool removePlacement(const QString &placementId, QString *errorMessage = nullptr);
    bool updateToolFields(const QString &toolId,
                          const qtllm::toolsstudio::ToolStudioEditableFields &fields,
                          QString *errorMessage = nullptr);
    bool exportWorkspace(const QString &filePath, bool includeToolSnapshots, QString *errorMessage = nullptr);
    bool exportSubtree(const QString &nodeId, const QString &filePath, bool includeToolSnapshots, QString *errorMessage = nullptr);
    std::optional<qtllm::toolsstudio::ToolMergePreview> buildImportPreview(const QString &filePath, QString *errorMessage = nullptr) const;
    bool applyImportPreview(const qtllm::toolsstudio::ToolMergePreview &preview, QString *errorMessage = nullptr);

signals:
    void stateChanged();
    void errorOccurred(const QString &message);
    void statusMessage(const QString &message);

private:
    std::unique_ptr<qtllm::toolsstudio::ToolCatalogService> m_catalogService;
    std::unique_ptr<qtllm::toolsstudio::ToolImportExportService> m_importExportService;
    std::unique_ptr<qtllm::toolsstudio::ToolMergeService> m_mergeService;
    std::unique_ptr<qtllm::toolsstudio::ToolWorkspaceService> m_workspaceService;
    std::unique_ptr<qtllm::toolsstudio::ToolStudioSettings> m_settings;
};
