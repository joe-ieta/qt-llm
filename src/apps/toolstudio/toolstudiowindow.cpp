#include "toolstudiowindow.h"

#include "toolsettingsdialog.h"
#include "toolimportpreviewdialog.h"

#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QColor>
#include <QDialog>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFormLayout>
#include <QFileDialog>
#include <QHeaderView>
#include <QInputDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QMenuBar>
#include <QMimeData>
#include <QMessageBox>
#include <QPalette>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSet>
#include <QSplitter>
#include <QStatusBar>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextEdit>
#include <QToolBar>
#include <QTreeWidgetItemIterator>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <functional>

namespace {

const char kCatalogSelectionId[] = "__catalog__";
const char kToolStudioDragMimeType[] = "application/x-toolstudio-item";

QPoint eventPosition(const QDropEvent *event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return event->position().toPoint();
#else
    return event->pos();
#endif
}

QStringList splitTags(const QString &value)
{
    QStringList parts = value.split(',', Qt::SkipEmptyParts);
    for (QString &part : parts) {
        part = part.trimmed();
    }
    parts.removeAll(QString());
    return parts;
}

QPalette makePalette(const QString &theme)
{
    if (theme == QStringLiteral("dark")) {
        QPalette palette;
        palette.setColor(QPalette::Window, QColor(QStringLiteral("#20242b")));
        palette.setColor(QPalette::WindowText, Qt::white);
        palette.setColor(QPalette::Base, QColor(QStringLiteral("#171a1f")));
        palette.setColor(QPalette::AlternateBase, QColor(QStringLiteral("#2a3038")));
        palette.setColor(QPalette::Text, Qt::white);
        palette.setColor(QPalette::Button, QColor(QStringLiteral("#2b313a")));
        palette.setColor(QPalette::ButtonText, Qt::white);
        palette.setColor(QPalette::Highlight, QColor(QStringLiteral("#3b82f6")));
        palette.setColor(QPalette::HighlightedText, Qt::white);
        return palette;
    }

    if (theme == QStringLiteral("sepia")) {
        QPalette palette;
        palette.setColor(QPalette::Window, QColor(QStringLiteral("#f5eddc")));
        palette.setColor(QPalette::WindowText, QColor(QStringLiteral("#3b3126")));
        palette.setColor(QPalette::Base, QColor(QStringLiteral("#fffaf0")));
        palette.setColor(QPalette::AlternateBase, QColor(QStringLiteral("#efe3cb")));
        palette.setColor(QPalette::Text, QColor(QStringLiteral("#3b3126")));
        palette.setColor(QPalette::Button, QColor(QStringLiteral("#e5d0a6")));
        palette.setColor(QPalette::ButtonText, QColor(QStringLiteral("#3b3126")));
        palette.setColor(QPalette::Highlight, QColor(QStringLiteral("#ba7a2f")));
        palette.setColor(QPalette::HighlightedText, Qt::white);
        return palette;
    }

    if (theme == QStringLiteral("light")) {
        return QApplication::style()->standardPalette();
    }

    return QApplication::palette();
}

class ToolStudioTreeWidget final : public QTreeWidget
{
public:
    explicit ToolStudioTreeWidget(QWidget *parent = nullptr)
        : QTreeWidget(parent)
    {
        setDragEnabled(true);
        setAcceptDrops(true);
        setDropIndicatorShown(true);
        viewport()->setAcceptDrops(true);
    }

    void setIdentifiers(const QString &catalogSelectionId, const QString &workspaceRootId)
    {
        m_catalogSelectionId = catalogSelectionId;
        m_workspaceRootId = workspaceRootId;
    }

    std::function<void(const QString &, const QString &, int)> onCategoryDrop;
    std::function<void(const QString &, const QString &, const QString &)> onToolDropToCategory;

protected:
    void startDrag(Qt::DropActions supportedActions) override
    {
        Q_UNUSED(supportedActions)
        QTreeWidgetItem *item = currentItem();
        if (!item) {
            return;
        }

        const QString nodeId = item->data(0, Qt::UserRole).toString();
        if (nodeId.isEmpty() || nodeId == m_catalogSelectionId || nodeId == m_workspaceRootId) {
            return;
        }

        QTreeWidget::startDrag(Qt::MoveAction);
    }

    void dragEnterEvent(QDragEnterEvent *event) override
    {
        if (event->mimeData()->hasFormat(QString::fromLatin1(kToolStudioDragMimeType))
            || event->source() == this) {
            event->acceptProposedAction();
            return;
        }
        QTreeWidget::dragEnterEvent(event);
    }

    void dragMoveEvent(QDragMoveEvent *event) override
    {
        QString targetNodeId;
        int insertIndex = -1;
        if ((event->source() == this || event->mimeData()->hasFormat(QString::fromLatin1(kToolStudioDragMimeType)))
            && computeDropTarget(eventPosition(event), &targetNodeId, &insertIndex, event->mimeData()->hasFormat(QString::fromLatin1(kToolStudioDragMimeType)))) {
            event->acceptProposedAction();
            return;
        }
        QTreeWidget::dragMoveEvent(event);
    }

    void dropEvent(QDropEvent *event) override
    {
        QString targetNodeId;
        int insertIndex = -1;
        const bool externalToolDrop = event->mimeData()->hasFormat(QString::fromLatin1(kToolStudioDragMimeType));
        if (!computeDropTarget(eventPosition(event), &targetNodeId, &insertIndex, externalToolDrop)) {
            event->ignore();
            return;
        }

        if (externalToolDrop) {
            QJsonParseError parseError;
            const QJsonDocument payload = QJsonDocument::fromJson(
                event->mimeData()->data(QString::fromLatin1(kToolStudioDragMimeType)), &parseError);
            if (parseError.error != QJsonParseError::NoError || !payload.isObject()) {
                event->ignore();
                return;
            }

            const QJsonObject object = payload.object();
            if (onToolDropToCategory) {
                onToolDropToCategory(object.value(QStringLiteral("toolId")).toString(),
                                     object.value(QStringLiteral("placementId")).toString(),
                                     targetNodeId);
            }
            event->acceptProposedAction();
            return;
        }

        QTreeWidgetItem *item = currentItem();
        if (!item) {
            event->ignore();
            return;
        }

        const QString nodeId = item->data(0, Qt::UserRole).toString();
        if (nodeId.isEmpty() || nodeId == m_catalogSelectionId || nodeId == m_workspaceRootId) {
            event->ignore();
            return;
        }

        if (onCategoryDrop) {
            onCategoryDrop(nodeId, targetNodeId, insertIndex);
        }
        event->acceptProposedAction();
    }

private:
    bool computeDropTarget(const QPoint &pos,
                           QString *targetNodeId,
                           int *insertIndex,
                           bool externalToolDrop) const
    {
        if (!targetNodeId || !insertIndex) {
            return false;
        }

        QTreeWidgetItem *targetItem = itemAt(pos);
        DropIndicatorPosition position = dropIndicatorPosition();
        if (!targetItem) {
            targetItem = topLevelItemCount() > 1 ? topLevelItem(1) : nullptr;
            position = OnItem;
        }

        if (!targetItem) {
            return false;
        }

        const QString targetId = targetItem->data(0, Qt::UserRole).toString();
        if (targetId == m_catalogSelectionId) {
            return false;
        }

        if (externalToolDrop) {
            if (position == AboveItem || position == BelowItem) {
                return false;
            }
            *targetNodeId = targetId;
            *insertIndex = targetItem->childCount();
            return true;
        }

        if (position == OnItem || targetId == m_workspaceRootId) {
            *targetNodeId = targetId;
            *insertIndex = targetItem->childCount();
            return true;
        }

        QTreeWidgetItem *parentItem = targetItem->parent();
        if (!parentItem) {
            return false;
        }

        const QString parentId = parentItem->data(0, Qt::UserRole).toString();
        if (parentId == m_catalogSelectionId) {
            return false;
        }

        *targetNodeId = parentId;
        const int row = parentItem->indexOfChild(targetItem);
        *insertIndex = position == AboveItem ? row : row + 1;
        return true;
    }

private:
    QString m_catalogSelectionId;
    QString m_workspaceRootId;
};

class ToolStudioTableWidget final : public QTableWidget
{
public:
    explicit ToolStudioTableWidget(QWidget *parent = nullptr)
        : QTableWidget(parent)
    {
        setDragEnabled(true);
        setAcceptDrops(true);
        setDropIndicatorShown(true);
        viewport()->setAcceptDrops(true);
        setDragDropOverwriteMode(false);
    }

    void setBehaviorFlags(bool catalogMode, bool allowInternalReorder)
    {
        m_catalogMode = catalogMode;
        m_allowInternalReorder = allowInternalReorder;
        setDragDropMode(allowInternalReorder ? QAbstractItemView::DragDrop : QAbstractItemView::DragOnly);
        setDefaultDropAction(Qt::MoveAction);
    }

    std::function<void(const QString &, int)> onPlacementReorder;

protected:
    void startDrag(Qt::DropActions supportedActions) override
    {
        Q_UNUSED(supportedActions)
        const QList<QTableWidgetSelectionRange> ranges = selectedRanges();
        if (ranges.isEmpty()) {
            return;
        }

        const int row = ranges.first().topRow();
        QTableWidgetItem *item = this->item(row, 0);
        if (!item) {
            return;
        }

        const QString toolId = item->data(Qt::UserRole).toString();
        const QString placementId = item->data(Qt::UserRole + 1).toString();
        if (toolId.isEmpty()) {
            return;
        }

        QJsonObject payload;
        payload.insert(QStringLiteral("toolId"), toolId);
        payload.insert(QStringLiteral("placementId"), placementId);
        payload.insert(QStringLiteral("catalogMode"), m_catalogMode);

        auto *drag = new QDrag(this);
        auto *mimeData = new QMimeData();
        mimeData->setData(QString::fromLatin1(kToolStudioDragMimeType),
                          QJsonDocument(payload).toJson(QJsonDocument::Compact));
        drag->setMimeData(mimeData);
        drag->exec(Qt::MoveAction);
    }

    void dragEnterEvent(QDragEnterEvent *event) override
    {
        if (event->source() == this && m_allowInternalReorder) {
            event->acceptProposedAction();
            return;
        }
        event->ignore();
    }

    void dragMoveEvent(QDragMoveEvent *event) override
    {
        if (event->source() == this && m_allowInternalReorder) {
            event->acceptProposedAction();
            return;
        }
        event->ignore();
    }

    void dropEvent(QDropEvent *event) override
    {
        if (event->source() != this || !m_allowInternalReorder) {
            event->ignore();
            return;
        }

        const QList<QTableWidgetSelectionRange> ranges = selectedRanges();
        if (ranges.isEmpty()) {
            event->ignore();
            return;
        }

        const int sourceRow = ranges.first().topRow();
        QTableWidgetItem *item = this->item(sourceRow, 0);
        if (!item) {
            event->ignore();
            return;
        }

        const QString placementId = item->data(Qt::UserRole + 1).toString();
        if (placementId.isEmpty()) {
            event->ignore();
            return;
        }

        int targetRow = rowAt(eventPosition(event).y());
        if (targetRow < 0) {
            targetRow = rowCount();
        } else if (dropIndicatorPosition() == BelowItem) {
            ++targetRow;
        }

        if (onPlacementReorder) {
            onPlacementReorder(placementId, targetRow);
        }
        event->acceptProposedAction();
    }

private:
    bool m_catalogMode = false;
    bool m_allowInternalReorder = false;
};

} // namespace

ToolStudioWindow::ToolStudioWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_controller(new ToolStudioController(this))
    , m_tree(new ToolStudioTreeWidget(this))
    , m_table(new ToolStudioTableWidget(this))
    , m_includeDescendantsCheck(new QCheckBox(QStringLiteral("Include descendants"), this))
    , m_nameEdit(new QLineEdit(this))
    , m_toolIdEdit(new QLineEdit(this))
    , m_invocationEdit(new QLineEdit(this))
    , m_globalTagsEdit(new QLineEdit(this))
    , m_enabledCheck(new QCheckBox(QStringLiteral("Enabled"), this))
    , m_descriptionEdit(new QTextEdit(this))
    , m_schemaEdit(new QPlainTextEdit(this))
    , m_localTagsEdit(new QLineEdit(this))
    , m_noteEdit(new QTextEdit(this))
    , m_pinnedCheck(new QCheckBox(QStringLiteral("Pinned"), this))
    , m_saveAction(nullptr)
    , m_importAction(nullptr)
    , m_exportWorkspaceAction(nullptr)
    , m_exportSubtreeAction(nullptr)
    , m_createWorkspaceAction(nullptr)
    , m_openWorkspaceAction(nullptr)
    , m_removeWorkspaceAction(nullptr)
    , m_createCategoryAction(nullptr)
    , m_renameCategoryAction(nullptr)
    , m_moveCategoryAction(nullptr)
    , m_moveCategoryUpAction(nullptr)
    , m_moveCategoryDownAction(nullptr)
    , m_removeCategoryAction(nullptr)
    , m_addToolAction(nullptr)
    , m_movePlacementAction(nullptr)
    , m_movePlacementUpAction(nullptr)
    , m_movePlacementDownAction(nullptr)
    , m_removePlacementAction(nullptr)
    , m_settingsAction(nullptr)
{
    setWindowTitle(QStringLiteral("Tool Studio"));
    resize(1520, 900);

    buildUi();
    buildMenus();
    configureDragDrop();

    connect(m_controller, &ToolStudioController::stateChanged, this, &ToolStudioWindow::refreshUi);
    connect(m_controller, &ToolStudioController::errorOccurred, this, &ToolStudioWindow::showError);
    connect(m_controller, &ToolStudioController::statusMessage, this, &ToolStudioWindow::showStatus);
    connect(m_tree, &QTreeWidget::itemSelectionChanged, this, &ToolStudioWindow::onTreeSelectionChanged);
    connect(m_table, &QTableWidget::itemSelectionChanged, this, &ToolStudioWindow::onTableSelectionChanged);
    connect(m_includeDescendantsCheck, &QCheckBox::toggled, this, &ToolStudioWindow::populateTable);

    QString error;
    m_controller->initialize(&error);
    m_includeDescendantsCheck->setChecked(m_controller->settings()->includeDescendantsByDefault());
    refreshUi();
    applySettings();
}

ToolStudioWindow::~ToolStudioWindow() = default;

void ToolStudioWindow::closeEvent(QCloseEvent *event)
{
    m_controller->settings()->setMainWindowGeometry(saveGeometry());
    m_controller->settings()->setMainWindowState(saveState());
    m_controller->settings()->setIncludeDescendantsByDefault(m_includeDescendantsCheck->isChecked());
    QMainWindow::closeEvent(event);
}

void ToolStudioWindow::refreshUi()
{
    populateTree();
    populateTable();
    populateEditors();
    const auto workspace = m_controller->workspaceService()->currentWorkspace();
    setWindowTitle(QStringLiteral("Tool Studio - %1").arg(workspace.name.isEmpty() ? workspace.workspaceId : workspace.name));
}

void ToolStudioWindow::onTreeSelectionChanged()
{
    populateTable();
    populateEditors();
}

void ToolStudioWindow::onTableSelectionChanged()
{
    populateEditors();
}

void ToolStudioWindow::createWorkspace()
{
    bool ok = false;
    const QString name = QInputDialog::getText(this,
                                               QStringLiteral("New Workspace"),
                                               QStringLiteral("Workspace name"),
                                               QLineEdit::Normal,
                                               QStringLiteral("Workspace"),
                                               &ok);
    if (!ok) {
        return;
    }

    QString workspaceId;
    QString error;
    if (m_controller->createWorkspace(name, &workspaceId, &error)) {
        showStatus(QStringLiteral("Created workspace %1").arg(workspaceId));
    } else if (!error.isEmpty()) {
        showError(error);
    }
}

void ToolStudioWindow::removeWorkspace()
{
    const QVector<qtllm::toolsstudio::ToolWorkspaceIndexEntry> entries =
        m_controller->workspaceService()->workspaceEntries(nullptr);
    QStringList choices;
    const QString currentWorkspaceId = m_controller->workspaceService()->currentWorkspaceId();
    for (const qtllm::toolsstudio::ToolWorkspaceIndexEntry &entry : entries) {
        if (entry.workspaceId != currentWorkspaceId) {
            choices.append(entry.workspaceId + QStringLiteral(" | ") + entry.name);
        }
    }

    if (choices.isEmpty()) {
        showError(QStringLiteral("No removable workspace is available."));
        return;
    }

    bool ok = false;
    const QString chosen = QInputDialog::getItem(this,
                                                 QStringLiteral("Remove Workspace"),
                                                 QStringLiteral("Select a workspace to remove"),
                                                 choices,
                                                 0,
                                                 false,
                                                 &ok);
    if (!ok || chosen.isEmpty()) {
        return;
    }

    const QString workspaceId = chosen.section(QStringLiteral(" | "), 0, 0);
    if (QMessageBox::question(this,
                              QStringLiteral("Remove Workspace"),
                              QStringLiteral("Remove workspace %1?").arg(workspaceId))
        != QMessageBox::Yes) {
        return;
    }

    QString error;
    if (!m_controller->removeWorkspace(workspaceId, &error) && !error.isEmpty()) {
        showError(error);
    }
}

void ToolStudioWindow::importPackage()
{
    const QString filePath = QFileDialog::getOpenFileName(this,
                                                          QStringLiteral("Import Tool Studio Package"),
                                                          QString(),
                                                          QStringLiteral("Tool Studio Package (*.toolstudio.json);;JSON Files (*.json)"));
    if (filePath.isEmpty()) {
        return;
    }

    QString error;
    const std::optional<qtllm::toolsstudio::ToolMergePreview> preview =
        m_controller->buildImportPreview(filePath, &error);
    if (!preview.has_value()) {
        if (!error.isEmpty()) {
            showError(error);
        }
        return;
    }

    ToolImportPreviewDialog dialog(*preview, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    if (!m_controller->applyImportPreview(*preview, &error) && !error.isEmpty()) {
        showError(error);
    }
}

void ToolStudioWindow::exportWorkspace()
{
    const QString filePath = QFileDialog::getSaveFileName(this,
                                                          QStringLiteral("Export Workspace"),
                                                          QStringLiteral("workspace.toolstudio.json"),
                                                          QStringLiteral("Tool Studio Package (*.toolstudio.json)"));
    if (filePath.isEmpty()) {
        return;
    }

    QString error;
    if (!m_controller->exportWorkspace(filePath, true, &error) && !error.isEmpty()) {
        showError(error);
    }
}

void ToolStudioWindow::exportSelectedSubtree()
{
    if (isCatalogSelection()) {
        showError(QStringLiteral("Select a category subtree to export."));
        return;
    }

    const QString filePath = QFileDialog::getSaveFileName(this,
                                                          QStringLiteral("Export Selected Subtree"),
                                                          QStringLiteral("subtree.toolstudio.json"),
                                                          QStringLiteral("Tool Studio Package (*.toolstudio.json)"));
    if (filePath.isEmpty()) {
        return;
    }

    QString error;
    if (!m_controller->exportSubtree(currentNodeId(), filePath, true, &error) && !error.isEmpty()) {
        showError(error);
    }
}

void ToolStudioWindow::createCategory()
{
    const QString parentNodeId = isCatalogSelection()
        ? m_controller->workspaceService()->currentWorkspace().rootNodeId
        : currentNodeId();

    bool ok = false;
    const QString name = QInputDialog::getText(this,
                                               QStringLiteral("New Category"),
                                               QStringLiteral("Category name"),
                                               QLineEdit::Normal,
                                               QStringLiteral("Category"),
                                               &ok);
    if (!ok) {
        return;
    }

    QString nodeId;
    QString error;
    if (m_controller->createNode(parentNodeId, name, &nodeId, &error)) {
        showStatus(QStringLiteral("Created category %1").arg(nodeId));
    } else if (!error.isEmpty()) {
        showError(error);
    }
}

void ToolStudioWindow::openWorkspace()
{
    const QVector<qtllm::toolsstudio::ToolWorkspaceIndexEntry> entries =
        m_controller->workspaceService()->workspaceEntries(nullptr);
    if (entries.isEmpty()) {
        showError(QStringLiteral("No workspaces available."));
        return;
    }

    QStringList choices;
    for (const qtllm::toolsstudio::ToolWorkspaceIndexEntry &entry : entries) {
        choices.append(entry.workspaceId + QStringLiteral(" | ") + entry.name);
    }
    choices.sort(Qt::CaseInsensitive);

    bool ok = false;
    const QString chosen = QInputDialog::getItem(this,
                                                 QStringLiteral("Open Workspace"),
                                                 QStringLiteral("Select a workspace"),
                                                 choices,
                                                 0,
                                                 false,
                                                 &ok);
    if (!ok || chosen.isEmpty()) {
        return;
    }

    const QString workspaceId = chosen.section(QStringLiteral(" | "), 0, 0);
    QString error;
    if (!m_controller->openWorkspace(workspaceId, &error) && !error.isEmpty()) {
        showError(error);
    }
}

void ToolStudioWindow::renameCategory()
{
    if (isCatalogSelection()) {
        showError(QStringLiteral("Select a category to rename."));
        return;
    }

    const auto nodeOpt = m_controller->workspaceService()->findNode(currentNodeId());
    if (!nodeOpt.has_value() || nodeOpt->nodeId == m_controller->workspaceService()->currentWorkspace().rootNodeId) {
        showError(QStringLiteral("Select a non-root category to rename."));
        return;
    }

    bool ok = false;
    const QString name = QInputDialog::getText(this,
                                               QStringLiteral("Rename Category"),
                                               QStringLiteral("Category name"),
                                               QLineEdit::Normal,
                                               nodeOpt->name,
                                               &ok);
    if (!ok) {
        return;
    }

    qtllm::toolsstudio::ToolCategoryNode node = *nodeOpt;
    node.name = name;
    QString error;
    if (!m_controller->updateNode(node, &error) && !error.isEmpty()) {
        showError(error);
    }
}

void ToolStudioWindow::moveCategoryTo()
{
    if (isCatalogSelection()) {
        showError(QStringLiteral("Select a category to move."));
        return;
    }

    const auto nodeOpt = m_controller->workspaceService()->findNode(currentNodeId());
    if (!nodeOpt.has_value() || nodeOpt->nodeId == m_controller->workspaceService()->currentWorkspace().rootNodeId) {
        showError(QStringLiteral("Select a non-root category to move."));
        return;
    }

    QSet<QString> blockedIds;
    blockedIds.insert(nodeOpt->nodeId);
    QVector<QString> stack;
    stack.append(nodeOpt->nodeId);
    const QVector<qtllm::toolsstudio::ToolCategoryNode> allNodes = m_controller->workspaceService()->allNodes();
    while (!stack.isEmpty()) {
        const QString current = stack.takeLast();
        for (const qtllm::toolsstudio::ToolCategoryNode &node : allNodes) {
            if (node.parentNodeId == current && !blockedIds.contains(node.nodeId)) {
                blockedIds.insert(node.nodeId);
                stack.append(node.nodeId);
            }
        }
    }

    QStringList choices;
    for (const qtllm::toolsstudio::ToolCategoryNode &node : allNodes) {
        if (!blockedIds.contains(node.nodeId)) {
            choices.append(node.nodeId + QStringLiteral(" | ") + node.name);
        }
    }
    choices.sort(Qt::CaseInsensitive);

    bool ok = false;
    const QString chosen = QInputDialog::getItem(this,
                                                 QStringLiteral("Move Category"),
                                                 QStringLiteral("Target parent category"),
                                                 choices,
                                                 0,
                                                 false,
                                                 &ok);
    if (!ok || chosen.isEmpty()) {
        return;
    }

    const QString targetNodeId = chosen.section(QStringLiteral(" | "), 0, 0);
    if (targetNodeId.isEmpty()) {
        return;
    }

    const int newOrder = m_controller->workspaceService()->childNodes(targetNodeId).size() * 10;
    QString error;
    if (!m_controller->moveNode(nodeOpt->nodeId, targetNodeId, newOrder, &error) && !error.isEmpty()) {
        showError(error);
    }
}

void ToolStudioWindow::moveCategoryUp()
{
    if (isCatalogSelection()) {
        showError(QStringLiteral("Select a category to move."));
        return;
    }

    const auto nodeOpt = m_controller->workspaceService()->findNode(currentNodeId());
    if (!nodeOpt.has_value()) {
        return;
    }

    QVector<qtllm::toolsstudio::ToolCategoryNode> siblings =
        m_controller->workspaceService()->childNodes(nodeOpt->parentNodeId);
    for (int i = 1; i < siblings.size(); ++i) {
        if (siblings.at(i).nodeId != nodeOpt->nodeId) {
            continue;
        }

        const qtllm::toolsstudio::ToolCategoryNode prev = siblings.at(i - 1);
        QString error;
        if (!m_controller->moveNode(prev.nodeId, prev.parentNodeId, nodeOpt->order, &error)) {
            if (!error.isEmpty()) {
                showError(error);
            }
            return;
        }
        if (!m_controller->moveNode(nodeOpt->nodeId, nodeOpt->parentNodeId, prev.order, &error) && !error.isEmpty()) {
            showError(error);
        }
        return;
    }
}

void ToolStudioWindow::moveCategoryDown()
{
    if (isCatalogSelection()) {
        showError(QStringLiteral("Select a category to move."));
        return;
    }

    const auto nodeOpt = m_controller->workspaceService()->findNode(currentNodeId());
    if (!nodeOpt.has_value()) {
        return;
    }

    QVector<qtllm::toolsstudio::ToolCategoryNode> siblings =
        m_controller->workspaceService()->childNodes(nodeOpt->parentNodeId);
    for (int i = 0; i + 1 < siblings.size(); ++i) {
        if (siblings.at(i).nodeId != nodeOpt->nodeId) {
            continue;
        }

        const qtllm::toolsstudio::ToolCategoryNode next = siblings.at(i + 1);
        QString error;
        if (!m_controller->moveNode(next.nodeId, next.parentNodeId, nodeOpt->order, &error)) {
            if (!error.isEmpty()) {
                showError(error);
            }
            return;
        }
        if (!m_controller->moveNode(nodeOpt->nodeId, nodeOpt->parentNodeId, next.order, &error) && !error.isEmpty()) {
            showError(error);
        }
        return;
    }
}

void ToolStudioWindow::removeCategory()
{
    if (isCatalogSelection()) {
        showError(QStringLiteral("Select a category to remove."));
        return;
    }

    const QString nodeId = currentNodeId();
    if (nodeId.isEmpty() || nodeId == m_controller->workspaceService()->currentWorkspace().rootNodeId) {
        showError(QStringLiteral("Select a non-root category to remove."));
        return;
    }

    if (QMessageBox::question(this,
                              QStringLiteral("Remove Category"),
                              QStringLiteral("Remove the selected category and all nested categories?"))
        != QMessageBox::Yes) {
        return;
    }

    QString error;
    if (!m_controller->removeNode(nodeId, true, &error) && !error.isEmpty()) {
        showError(error);
    }
}

void ToolStudioWindow::addToolToCategory()
{
    if (isCatalogSelection()) {
        showError(QStringLiteral("Select a category, not the catalog view."));
        return;
    }

    const QString nodeId = currentNodeId();
    if (nodeId.isEmpty()) {
        showError(QStringLiteral("Select a category first."));
        return;
    }

    QStringList choices;
    const QList<qtllm::tools::LlmToolDefinition> tools = m_controller->catalogService()->allTools();
    for (const qtllm::tools::LlmToolDefinition &tool : tools) {
        choices.append(tool.toolId + QStringLiteral(" | ") + tool.name);
    }
    choices.sort(Qt::CaseInsensitive);

    bool ok = false;
    const QString chosen = QInputDialog::getItem(this,
                                                 QStringLiteral("Add Tool"),
                                                 QStringLiteral("Select a tool"),
                                                 choices,
                                                 0,
                                                 false,
                                                 &ok);
    if (!ok || chosen.isEmpty()) {
        return;
    }

    const QString toolId = chosen.section(QStringLiteral(" | "), 0, 0);
    QString error;
    if (!m_controller->addToolToNode(nodeId, toolId, nullptr, &error) && !error.isEmpty()) {
        showError(error);
    }
}

void ToolStudioWindow::moveSelectedPlacementToCategory()
{
    const QString placementId = selectedPlacementId();
    if (placementId.isEmpty()) {
        showError(QStringLiteral("Select a categorized tool row first."));
        return;
    }

    const QString targetNodeId = chooseCategory(QStringLiteral("Move Tool"),
                                                QStringLiteral("Target category"));
    if (targetNodeId.isEmpty()) {
        return;
    }

    const int newOrder = m_controller->workspaceService()->placementsInNode(targetNodeId, false).size() * 10;
    QString error;
    if (!m_controller->movePlacement(placementId, targetNodeId, newOrder, &error) && !error.isEmpty()) {
        showError(error);
    }
}

void ToolStudioWindow::moveSelectedPlacementUp()
{
    const QString placementId = selectedPlacementId();
    if (placementId.isEmpty()) {
        showError(QStringLiteral("Select a categorized tool row first."));
        return;
    }

    const auto placementOpt = m_controller->workspaceService()->findPlacement(placementId);
    if (!placementOpt.has_value()) {
        return;
    }

    QVector<qtllm::toolsstudio::ToolPlacement> placements =
        m_controller->workspaceService()->placementsInNode(placementOpt->nodeId, false);
    for (int i = 1; i < placements.size(); ++i) {
        if (placements.at(i).placementId != placementId) {
            continue;
        }

        const qtllm::toolsstudio::ToolPlacement prev = placements.at(i - 1);
        QString error;
        if (!m_controller->movePlacement(prev.placementId, prev.nodeId, placementOpt->order, &error)) {
            if (!error.isEmpty()) {
                showError(error);
            }
            return;
        }
        if (!m_controller->movePlacement(placementId, placementOpt->nodeId, prev.order, &error) && !error.isEmpty()) {
            showError(error);
        }
        return;
    }
}

void ToolStudioWindow::moveSelectedPlacementDown()
{
    const QString placementId = selectedPlacementId();
    if (placementId.isEmpty()) {
        showError(QStringLiteral("Select a categorized tool row first."));
        return;
    }

    const auto placementOpt = m_controller->workspaceService()->findPlacement(placementId);
    if (!placementOpt.has_value()) {
        return;
    }

    QVector<qtllm::toolsstudio::ToolPlacement> placements =
        m_controller->workspaceService()->placementsInNode(placementOpt->nodeId, false);
    for (int i = 0; i + 1 < placements.size(); ++i) {
        if (placements.at(i).placementId != placementId) {
            continue;
        }

        const qtllm::toolsstudio::ToolPlacement next = placements.at(i + 1);
        QString error;
        if (!m_controller->movePlacement(next.placementId, next.nodeId, placementOpt->order, &error)) {
            if (!error.isEmpty()) {
                showError(error);
            }
            return;
        }
        if (!m_controller->movePlacement(placementId, placementOpt->nodeId, next.order, &error) && !error.isEmpty()) {
            showError(error);
        }
        return;
    }
}

void ToolStudioWindow::removeSelectedPlacement()
{
    const QString placementId = selectedPlacementId();
    if (placementId.isEmpty()) {
        showError(QStringLiteral("Select a categorized tool row first."));
        return;
    }

    QString error;
    if (!m_controller->removePlacement(placementId, &error) && !error.isEmpty()) {
        showError(error);
    }
}

void ToolStudioWindow::saveEdits()
{
    const QString toolId = selectedToolId();
    if (toolId.isEmpty()) {
        showError(QStringLiteral("Select a tool first."));
        return;
    }

    QString error;
    qtllm::toolsstudio::ToolStudioEditableFields fields;
    if (!currentToolFieldsFromUi(&fields, &error)) {
        showError(error);
        return;
    }

    if (!m_controller->updateToolFields(toolId, fields, &error)) {
        if (!error.isEmpty()) {
            showError(error);
        }
        return;
    }

    const QString placementId = selectedPlacementId();
    if (!placementId.isEmpty()) {
        const auto placementOpt = m_controller->workspaceService()->findPlacement(placementId);
        if (placementOpt.has_value()) {
            qtllm::toolsstudio::ToolPlacement placement = currentPlacementFromUi();
            if (!m_controller->updatePlacement(placement, &error) && !error.isEmpty()) {
                showError(error);
                return;
            }
        }
    }

    m_controller->saveAll(&error);
}

void ToolStudioWindow::saveAll()
{
    QString error;
    if (!m_controller->saveAll(&error) && !error.isEmpty()) {
        showError(error);
    }
}

void ToolStudioWindow::openSettings()
{
    ToolSettingsDialog dialog(m_controller->settings(), this);
    if (dialog.exec() == QDialog::Accepted) {
        m_includeDescendantsCheck->setChecked(m_controller->settings()->includeDescendantsByDefault());
        applySettings();
        refreshUi();
    }
}

void ToolStudioWindow::buildUi()
{
    m_tree->setHeaderLabel(QStringLiteral("Categories"));
    m_table->setColumnCount(6);
    m_table->setHorizontalHeaderLabels(QStringList({
        QStringLiteral("Name"),
        QStringLiteral("Tool ID"),
        QStringLiteral("Invocation"),
        QStringLiteral("Source"),
        QStringLiteral("Enabled"),
        QStringLiteral("Local Tags")
    }));
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);

    m_toolIdEdit->setReadOnly(true);
    m_invocationEdit->setReadOnly(true);

    auto *leftPane = new QWidget(this);
    auto *leftLayout = new QVBoxLayout(leftPane);
    leftLayout->addWidget(new QLabel(QStringLiteral("Classification"), leftPane));
    leftLayout->addWidget(m_tree, 1);

    auto *centerPane = new QWidget(this);
    auto *centerLayout = new QVBoxLayout(centerPane);
    centerLayout->addWidget(m_includeDescendantsCheck);
    centerLayout->addWidget(m_table, 1);

    auto *form = new QFormLayout();
    form->addRow(QStringLiteral("Name"), m_nameEdit);
    form->addRow(QStringLiteral("Tool ID"), m_toolIdEdit);
    form->addRow(QStringLiteral("Invocation"), m_invocationEdit);
    form->addRow(QStringLiteral("Global Tags"), m_globalTagsEdit);
    form->addRow(QStringLiteral("Description"), m_descriptionEdit);
    form->addRow(QStringLiteral("Input Schema"), m_schemaEdit);
    form->addRow(QStringLiteral("Local Tags"), m_localTagsEdit);
    form->addRow(QStringLiteral("Placement Note"), m_noteEdit);

    auto *rightPane = new QWidget(this);
    auto *rightLayout = new QVBoxLayout(rightPane);
    rightLayout->addWidget(m_enabledCheck);
    rightLayout->addLayout(form);
    rightLayout->addWidget(m_pinnedCheck);
    auto *saveButton = new QPushButton(QStringLiteral("Save Edits"), rightPane);
    connect(saveButton, &QPushButton::clicked, this, &ToolStudioWindow::saveEdits);
    rightLayout->addWidget(saveButton);

    auto *splitter = new QSplitter(this);
    splitter->addWidget(leftPane);
    splitter->addWidget(centerPane);
    splitter->addWidget(rightPane);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setStretchFactor(2, 1);
    setCentralWidget(splitter);
    setStatusBar(new QStatusBar(this));
}

void ToolStudioWindow::buildMenus()
{
    auto *fileMenu = menuBar()->addMenu(QStringLiteral("&File"));
    m_importAction = fileMenu->addAction(QStringLiteral("Import Package"));
    m_exportWorkspaceAction = fileMenu->addAction(QStringLiteral("Export Workspace"));
    m_exportSubtreeAction = fileMenu->addAction(QStringLiteral("Export Selected Subtree"));
    fileMenu->addSeparator();
    m_createWorkspaceAction = fileMenu->addAction(QStringLiteral("New Workspace"));
    m_openWorkspaceAction = fileMenu->addAction(QStringLiteral("Open Workspace"));
    m_removeWorkspaceAction = fileMenu->addAction(QStringLiteral("Remove Workspace"));
    m_saveAction = fileMenu->addAction(QStringLiteral("Save All"));
    fileMenu->addSeparator();
    m_settingsAction = fileMenu->addAction(QStringLiteral("Settings"));

    auto *categoryMenu = menuBar()->addMenu(QStringLiteral("&Category"));
    m_createCategoryAction = categoryMenu->addAction(QStringLiteral("New Category"));
    m_renameCategoryAction = categoryMenu->addAction(QStringLiteral("Rename Category"));
    m_moveCategoryAction = categoryMenu->addAction(QStringLiteral("Move Category To..."));
    m_moveCategoryUpAction = categoryMenu->addAction(QStringLiteral("Move Category Up"));
    m_moveCategoryDownAction = categoryMenu->addAction(QStringLiteral("Move Category Down"));
    m_removeCategoryAction = categoryMenu->addAction(QStringLiteral("Remove Category"));
    m_addToolAction = categoryMenu->addAction(QStringLiteral("Add Tool To Category"));
    m_movePlacementAction = categoryMenu->addAction(QStringLiteral("Move Tool To Category..."));
    m_movePlacementUpAction = categoryMenu->addAction(QStringLiteral("Move Tool Up"));
    m_movePlacementDownAction = categoryMenu->addAction(QStringLiteral("Move Tool Down"));
    m_removePlacementAction = categoryMenu->addAction(QStringLiteral("Remove Tool From Category"));

    auto *toolbar = addToolBar(QStringLiteral("Main"));
    toolbar->addAction(m_importAction);
    toolbar->addAction(m_exportWorkspaceAction);
    toolbar->addAction(m_createWorkspaceAction);
    toolbar->addAction(m_openWorkspaceAction);
    toolbar->addAction(m_removeWorkspaceAction);
    toolbar->addAction(m_createCategoryAction);
    toolbar->addAction(m_renameCategoryAction);
    toolbar->addAction(m_addToolAction);
    toolbar->addAction(m_saveAction);

    connect(m_importAction, &QAction::triggered, this, &ToolStudioWindow::importPackage);
    connect(m_exportWorkspaceAction, &QAction::triggered, this, &ToolStudioWindow::exportWorkspace);
    connect(m_exportSubtreeAction, &QAction::triggered, this, &ToolStudioWindow::exportSelectedSubtree);
    connect(m_createWorkspaceAction, &QAction::triggered, this, &ToolStudioWindow::createWorkspace);
    connect(m_openWorkspaceAction, &QAction::triggered, this, &ToolStudioWindow::openWorkspace);
    connect(m_removeWorkspaceAction, &QAction::triggered, this, &ToolStudioWindow::removeWorkspace);
    connect(m_createCategoryAction, &QAction::triggered, this, &ToolStudioWindow::createCategory);
    connect(m_renameCategoryAction, &QAction::triggered, this, &ToolStudioWindow::renameCategory);
    connect(m_moveCategoryAction, &QAction::triggered, this, &ToolStudioWindow::moveCategoryTo);
    connect(m_moveCategoryUpAction, &QAction::triggered, this, &ToolStudioWindow::moveCategoryUp);
    connect(m_moveCategoryDownAction, &QAction::triggered, this, &ToolStudioWindow::moveCategoryDown);
    connect(m_removeCategoryAction, &QAction::triggered, this, &ToolStudioWindow::removeCategory);
    connect(m_addToolAction, &QAction::triggered, this, &ToolStudioWindow::addToolToCategory);
    connect(m_movePlacementAction, &QAction::triggered, this, &ToolStudioWindow::moveSelectedPlacementToCategory);
    connect(m_movePlacementUpAction, &QAction::triggered, this, &ToolStudioWindow::moveSelectedPlacementUp);
    connect(m_movePlacementDownAction, &QAction::triggered, this, &ToolStudioWindow::moveSelectedPlacementDown);
    connect(m_removePlacementAction, &QAction::triggered, this, &ToolStudioWindow::removeSelectedPlacement);
    connect(m_saveAction, &QAction::triggered, this, &ToolStudioWindow::saveAll);
    connect(m_settingsAction, &QAction::triggered, this, &ToolStudioWindow::openSettings);
}

void ToolStudioWindow::configureDragDrop()
{
    auto *tree = static_cast<ToolStudioTreeWidget *>(m_tree);
    tree->onCategoryDrop = [this](const QString &nodeId, const QString &targetParentId, int insertIndex) {
        handleCategoryDrop(nodeId, targetParentId, insertIndex);
    };
    tree->onToolDropToCategory = [this](const QString &toolId, const QString &placementId, const QString &targetNodeId) {
        handleToolDropToCategory(toolId, placementId, targetNodeId);
    };

    auto *table = static_cast<ToolStudioTableWidget *>(m_table);
    table->onPlacementReorder = [this](const QString &placementId, int insertIndex) {
        handlePlacementReorder(placementId, insertIndex);
    };
}

void ToolStudioWindow::applySettings()
{
    qApp->setStyle(QStringLiteral("Fusion"));
    qApp->setPalette(makePalette(m_controller->settings()->theme()));

    QFont uiFont = qApp->font();
    if (!m_controller->settings()->uiFontFamily().isEmpty()) {
        uiFont.setFamily(m_controller->settings()->uiFontFamily());
    }
    uiFont.setPointSize(m_controller->settings()->uiFontPointSize());
    qApp->setFont(uiFont);

    QFont editorFont(m_controller->settings()->editorFontFamily());
    editorFont.setPointSize(m_controller->settings()->editorFontPointSize());
    m_descriptionEdit->setFont(editorFont);
    m_schemaEdit->setFont(editorFont);
    m_noteEdit->setFont(editorFont);

    const QByteArray geometry = m_controller->settings()->mainWindowGeometry();
    if (!geometry.isEmpty()) {
        restoreGeometry(geometry);
    }
    const QByteArray state = m_controller->settings()->mainWindowState();
    if (!state.isEmpty()) {
        restoreState(state);
    }
}

void ToolStudioWindow::handleCategoryDrop(const QString &nodeId, const QString &targetParentId, int insertIndex)
{
    const auto nodeOpt = m_controller->workspaceService()->findNode(nodeId);
    if (!nodeOpt.has_value()) {
        return;
    }

    const QString oldParentId = nodeOpt->parentNodeId;
    QVector<qtllm::toolsstudio::ToolCategoryNode> oldSiblings =
        m_controller->workspaceService()->childNodes(oldParentId);
    QVector<qtllm::toolsstudio::ToolCategoryNode> targetSiblings =
        oldParentId == targetParentId
            ? oldSiblings
            : m_controller->workspaceService()->childNodes(targetParentId);

    QStringList oldOrder;
    for (const qtllm::toolsstudio::ToolCategoryNode &node : oldSiblings) {
        if (oldParentId != targetParentId && node.nodeId == nodeId) {
            continue;
        }
        oldOrder.append(node.nodeId);
    }

    QStringList targetOrder;
    for (const qtllm::toolsstudio::ToolCategoryNode &node : targetSiblings) {
        if (node.nodeId != nodeId) {
            targetOrder.append(node.nodeId);
        }
    }

    const int boundedIndex = qBound(0, insertIndex, targetOrder.size());
    targetOrder.insert(boundedIndex, nodeId);

    QString error;
    if (!m_controller->moveNode(nodeId, targetParentId, boundedIndex * 10, &error)) {
        if (!error.isEmpty()) {
            showError(error);
        }
        return;
    }

    if (oldParentId != targetParentId
        && !oldOrder.isEmpty()
        && !m_controller->reorderNodeChildren(oldParentId, oldOrder, &error)) {
        if (!error.isEmpty()) {
            showError(error);
        }
        return;
    }

    if (!m_controller->reorderNodeChildren(targetParentId, targetOrder, &error) && !error.isEmpty()) {
        showError(error);
    }
}

void ToolStudioWindow::handleToolDropToCategory(const QString &toolId,
                                                const QString &placementId,
                                                const QString &targetNodeId)
{
    QString error;
    if (!placementId.isEmpty()) {
        const QVector<qtllm::toolsstudio::ToolPlacement> placements =
            m_controller->workspaceService()->placementsInNode(targetNodeId, false);
        if (!m_controller->movePlacement(placementId, targetNodeId, placements.size() * 10, &error)) {
            if (!error.isEmpty()) {
                showError(error);
            }
        }
        return;
    }

    if (!m_controller->addToolToNode(targetNodeId, toolId, nullptr, &error) && !error.isEmpty()) {
        showError(error);
    }
}

void ToolStudioWindow::handlePlacementReorder(const QString &placementId, int insertIndex)
{
    if (isCatalogSelection() || m_includeDescendantsCheck->isChecked()) {
        return;
    }

    QString nodeId = currentNodeId();
    QVector<qtllm::toolsstudio::ToolPlacement> placements =
        m_controller->workspaceService()->placementsInNode(nodeId, false);
    QStringList orderedIds;
    for (const qtllm::toolsstudio::ToolPlacement &placement : placements) {
        if (placement.placementId != placementId) {
            orderedIds.append(placement.placementId);
        }
    }
    orderedIds.insert(qBound(0, insertIndex, orderedIds.size()), placementId);

    QString error;
    if (!m_controller->reorderPlacements(nodeId, orderedIds, &error) && !error.isEmpty()) {
        showError(error);
    }
}

void ToolStudioWindow::populateTree()
{
    const QString previousNodeId = currentNodeId();
    m_tree->clear();

    auto *catalogItem = new QTreeWidgetItem(QStringList(QStringLiteral("Catalog")));
    catalogItem->setData(0, Qt::UserRole, QString::fromLatin1(kCatalogSelectionId));
    catalogItem->setToolTip(0, QStringLiteral("Browse all canonical tools in the current catalog."));
    m_tree->addTopLevelItem(catalogItem);

    const auto workspace = m_controller->workspaceService()->currentWorkspace();
    static_cast<ToolStudioTreeWidget *>(m_tree)->setIdentifiers(QString::fromLatin1(kCatalogSelectionId),
                                                                workspace.rootNodeId);
    auto *rootItem = new QTreeWidgetItem(QStringList(workspace.name.isEmpty() ? workspace.workspaceId : workspace.name));
    rootItem->setData(0, Qt::UserRole, workspace.rootNodeId);
    m_tree->addTopLevelItem(rootItem);

    const auto addChildren = [&](auto &&self, const QString &parentId, QTreeWidgetItem *parentItem) -> void {
        const QVector<qtllm::toolsstudio::ToolCategoryNode> children = m_controller->workspaceService()->childNodes(parentId);
        for (const qtllm::toolsstudio::ToolCategoryNode &node : children) {
            auto *item = new QTreeWidgetItem(QStringList(node.name));
            item->setData(0, Qt::UserRole, node.nodeId);
            item->setToolTip(0, node.description);
            parentItem->addChild(item);
            self(self, node.nodeId, item);
            item->setExpanded(node.expanded);
        }
    };
    addChildren(addChildren, workspace.rootNodeId, rootItem);
    rootItem->setExpanded(true);

    QTreeWidgetItemIterator it(m_tree);
    while (*it) {
        if ((*it)->data(0, Qt::UserRole).toString() == previousNodeId) {
            m_tree->setCurrentItem(*it);
            break;
        }
        ++it;
    }
    if (!m_tree->currentItem()) {
        m_tree->setCurrentItem(catalogItem);
    }
}

void ToolStudioWindow::populateTable()
{
    const QString nodeId = currentNodeId();
    const QString previousToolId = selectedToolId();
    const bool showCatalog = isCatalogSelection();
    const bool allowInternalReorder = !showCatalog
        && !m_includeDescendantsCheck->isChecked()
        && m_controller->settings()->showBuiltinTools()
        && m_controller->settings()->showDisabledTools();
    static_cast<ToolStudioTableWidget *>(m_table)->setBehaviorFlags(showCatalog, allowInternalReorder);

    m_table->setRowCount(0);

    int row = 0;
    if (showCatalog) {
        const QList<qtllm::tools::LlmToolDefinition> tools = m_controller->catalogService()->allTools();
        for (const qtllm::tools::LlmToolDefinition &tool : tools) {
            if (!m_controller->settings()->showBuiltinTools() && tool.systemBuiltIn) {
                continue;
            }
            if (!m_controller->settings()->showDisabledTools() && !tool.enabled) {
                continue;
            }
            m_table->insertRow(row);
            auto *nameItem = new QTableWidgetItem(tool.name);
            nameItem->setData(Qt::UserRole, tool.toolId);
            nameItem->setData(Qt::UserRole + 1, QString());
            m_table->setItem(row, 0, nameItem);
            m_table->setItem(row, 1, new QTableWidgetItem(tool.toolId));
            m_table->setItem(row, 2, new QTableWidgetItem(tool.invocationName));
            m_table->setItem(row, 3, new QTableWidgetItem(sourceLabelForTool(tool)));
            m_table->setItem(row, 4, new QTableWidgetItem(tool.enabled ? QStringLiteral("Yes") : QStringLiteral("No")));
            m_table->setItem(row, 5, new QTableWidgetItem(QString()));
            ++row;
        }
    } else {
        const QVector<qtllm::toolsstudio::ToolPlacement> placements =
            m_controller->workspaceService()->placementsInNode(nodeId, m_includeDescendantsCheck->isChecked());
        for (const qtllm::toolsstudio::ToolPlacement &placement : placements) {
            const auto toolOpt = m_controller->catalogService()->findTool(placement.toolId);
            if (!toolOpt.has_value()) {
                continue;
            }

            const qtllm::tools::LlmToolDefinition &tool = *toolOpt;
            if (!m_controller->settings()->showBuiltinTools() && tool.systemBuiltIn) {
                continue;
            }
            if (!m_controller->settings()->showDisabledTools() && !tool.enabled) {
                continue;
            }
            m_table->insertRow(row);
            auto *nameItem = new QTableWidgetItem(tool.name);
            nameItem->setData(Qt::UserRole, tool.toolId);
            nameItem->setData(Qt::UserRole + 1, placement.placementId);
            m_table->setItem(row, 0, nameItem);
            m_table->setItem(row, 1, new QTableWidgetItem(tool.toolId));
            m_table->setItem(row, 2, new QTableWidgetItem(tool.invocationName));
            m_table->setItem(row, 3, new QTableWidgetItem(sourceLabelForTool(tool)));
            m_table->setItem(row, 4, new QTableWidgetItem(tool.enabled ? QStringLiteral("Yes") : QStringLiteral("No")));
            m_table->setItem(row, 5, new QTableWidgetItem(placement.localTags.join(QStringLiteral(", "))));
            ++row;
        }
    }

    for (int i = 0; i < m_table->rowCount(); ++i) {
        if (m_table->item(i, 0) && m_table->item(i, 0)->data(Qt::UserRole).toString() == previousToolId) {
            m_table->selectRow(i);
            break;
        }
    }
}

void ToolStudioWindow::populateEditors()
{
    const QString toolId = selectedToolId();
    if (toolId.isEmpty()) {
        m_nameEdit->clear();
        m_toolIdEdit->clear();
        m_invocationEdit->clear();
        m_globalTagsEdit->clear();
        m_enabledCheck->setChecked(false);
        m_descriptionEdit->clear();
        m_schemaEdit->clear();
        m_localTagsEdit->clear();
        m_noteEdit->clear();
        m_pinnedCheck->setChecked(false);
        return;
    }

    const auto toolOpt = m_controller->catalogService()->findTool(toolId);
    if (!toolOpt.has_value()) {
        return;
    }

    const qtllm::tools::LlmToolDefinition &tool = *toolOpt;
    m_nameEdit->setText(tool.name);
    m_toolIdEdit->setText(tool.toolId);
    m_invocationEdit->setText(tool.invocationName);
    m_globalTagsEdit->setText(tool.capabilityTags.join(QStringLiteral(", ")));
    m_enabledCheck->setChecked(tool.enabled);
    m_descriptionEdit->setPlainText(tool.description);
    m_schemaEdit->setPlainText(QString::fromUtf8(QJsonDocument(tool.inputSchema).toJson(QJsonDocument::Indented)));

    const QString placementId = selectedPlacementId();
    if (placementId.isEmpty()) {
        m_localTagsEdit->clear();
        m_noteEdit->clear();
        m_pinnedCheck->setChecked(false);
        return;
    }

    const auto placementOpt = m_controller->workspaceService()->findPlacement(placementId);
    if (!placementOpt.has_value()) {
        return;
    }

    const qtllm::toolsstudio::ToolPlacement &placement = *placementOpt;
    m_localTagsEdit->setText(placement.localTags.join(QStringLiteral(", ")));
    m_noteEdit->setPlainText(placement.note);
    m_pinnedCheck->setChecked(placement.pinned);
}

QString ToolStudioWindow::currentNodeId() const
{
    if (!m_tree->currentItem()) {
        return m_controller->workspaceService()->currentWorkspace().rootNodeId;
    }
    return m_tree->currentItem()->data(0, Qt::UserRole).toString();
}

bool ToolStudioWindow::isCatalogSelection() const
{
    return currentNodeId() == QString::fromLatin1(kCatalogSelectionId);
}

QString ToolStudioWindow::chooseCategory(const QString &title, const QString &label, const QString &excludeNodeId) const
{
    QStringList choices;
    const QVector<qtllm::toolsstudio::ToolCategoryNode> nodes = m_controller->workspaceService()->allNodes();
    for (const qtllm::toolsstudio::ToolCategoryNode &node : nodes) {
        if (!excludeNodeId.isEmpty() && node.nodeId == excludeNodeId) {
            continue;
        }
        choices.append(node.nodeId + QStringLiteral(" | ") + node.name);
    }
    choices.sort(Qt::CaseInsensitive);

    bool ok = false;
    const QString chosen = QInputDialog::getItem(const_cast<ToolStudioWindow *>(this),
                                                 title,
                                                 label,
                                                 choices,
                                                 0,
                                                 false,
                                                 &ok);
    if (!ok || chosen.isEmpty()) {
        return QString();
    }
    return chosen.section(QStringLiteral(" | "), 0, 0);
}

QString ToolStudioWindow::selectedToolId() const
{
    const QList<QTableWidgetSelectionRange> ranges = m_table->selectedRanges();
    if (ranges.isEmpty()) {
        return QString();
    }
    const int row = ranges.first().topRow();
    return m_table->item(row, 0) ? m_table->item(row, 0)->data(Qt::UserRole).toString() : QString();
}

QString ToolStudioWindow::selectedPlacementId() const
{
    const QList<QTableWidgetSelectionRange> ranges = m_table->selectedRanges();
    if (ranges.isEmpty()) {
        return QString();
    }
    const int row = ranges.first().topRow();
    return m_table->item(row, 0) ? m_table->item(row, 0)->data(Qt::UserRole + 1).toString() : QString();
}

bool ToolStudioWindow::currentToolFieldsFromUi(qtllm::toolsstudio::ToolStudioEditableFields *fields,
                                               QString *errorMessage) const
{
    if (!fields) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Tool fields output is not available");
        }
        return false;
    }

    fields->name = m_nameEdit->text();
    fields->description = m_descriptionEdit->toPlainText();
    fields->capabilityTags = splitTags(m_globalTagsEdit->text());
    fields->enabled = m_enabledCheck->isChecked();

    const QString schemaText = m_schemaEdit->toPlainText().trimmed();
    if (schemaText.isEmpty()) {
        fields->inputSchema = QJsonObject();
        return true;
    }

    QJsonParseError parseError;
    const QJsonDocument schemaDoc = QJsonDocument::fromJson(schemaText.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !schemaDoc.isObject()) {
        if (errorMessage) {
            *errorMessage = parseError.error == QJsonParseError::NoError
                ? QStringLiteral("Input Schema must be a JSON object.")
                : QStringLiteral("Input Schema JSON parse error: ") + parseError.errorString();
        }
        return false;
    }

    fields->inputSchema = schemaDoc.object();
    return true;
}

qtllm::toolsstudio::ToolPlacement ToolStudioWindow::currentPlacementFromUi() const
{
    qtllm::toolsstudio::ToolPlacement placement = m_controller->workspaceService()->findPlacement(selectedPlacementId()).value();
    placement.localTags = splitTags(m_localTagsEdit->text());
    placement.note = m_noteEdit->toPlainText();
    placement.pinned = m_pinnedCheck->isChecked();
    return placement;
}

QString ToolStudioWindow::sourceLabelForTool(const qtllm::tools::LlmToolDefinition &tool) const
{
    if (tool.systemBuiltIn) {
        return QStringLiteral("Built-in");
    }
    if (tool.toolId.startsWith(QStringLiteral("mcp::"))) {
        return QStringLiteral("MCP");
    }
    return QStringLiteral("Catalog");
}

void ToolStudioWindow::showError(const QString &message)
{
    statusBar()->showMessage(message, 5000);
    QMessageBox::warning(this, QStringLiteral("Tool Studio"), message);
}

void ToolStudioWindow::showStatus(const QString &message)
{
    statusBar()->showMessage(message, 4000);
}
