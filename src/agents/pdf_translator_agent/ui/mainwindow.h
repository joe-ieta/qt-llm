#pragma once

#include <QJsonArray>
#include <QWidget>
#include <memory>

class QComboBox;
class QCloseEvent;
class QGroupBox;
class QLineEdit;
class QPushButton;
class QPlainTextEdit;
class QTextEdit;
class QTabWidget;
class QLabel;
class QTableWidget;
class QProgressBar;
class QSpinBox;

namespace pdftranslator::skills {
class SkillRegistry;
class ModelRouter;
namespace mcp {
class McpGateway;
}
}

namespace pdftranslator::pipeline {
class DocumentWorkflowController;
class BatchTranslationQueueController;
}

namespace pdftranslator::viewer {
class CompareReaderWidget;
}

namespace pdftranslator {

class MainWindow : public QWidget
{
    Q_OBJECT
public:
    explicit MainWindow(const std::shared_ptr<skills::SkillRegistry> &skillRegistry,
                        const std::shared_ptr<skills::ModelRouter> &modelRouter,
                        const std::shared_ptr<skills::mcp::McpGateway> &mcpGateway,
                        QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void onDetectLanguageClicked();
    void onTranslateFragmentClicked();
    void onBrowsePdfClicked();
    void onExtractPdfClicked();
    void onTranslateDocumentClicked();
    void onAddBatchDocumentsClicked();
    void onPauseResumeBatchClicked();
    void onRetrySelectedBatchClicked();
    void onTestLanguageDetectEndpointClicked();
    void onTestTranslateEndpointClicked();
    void onTestExtractMcpClicked();
    void onTestTermMcpClicked();
    void onOpenSelectedBatchResultClicked();
    void onOpenSelectedBatchFolderClicked();
    void onLoadSelectedBatchCompareClicked();
    void onClearFragmentHistoryClicked();
    void refreshBatchTable();

private:
    void closeEvent(QCloseEvent *event) override;
    void buildUi();
    void loadUiState();
    void saveUiState() const;
    void connectPersistenceSignals();
    void appendFragmentHistoryEntry(const QString &role, const QString &text);
    void refreshFragmentHistoryView();
    void applyLanguageDetectBinding();
    void applyTranslateBinding();
    void applyPdfExtractBinding();
    void applyTermBaseBinding();
    void refreshLlamaCppModelList(QComboBox *modelCombo, const QString &provider);
    QString defaultManifestPathForPdf(const QString &pdfPath) const;
    int selectedBatchRow() const;

private:
    std::shared_ptr<skills::SkillRegistry> m_skillRegistry;
    std::shared_ptr<skills::ModelRouter> m_modelRouter;
    std::shared_ptr<skills::mcp::McpGateway> m_mcpGateway;
    std::unique_ptr<pipeline::DocumentWorkflowController> m_workflowController;
    std::unique_ptr<pipeline::BatchTranslationQueueController> m_batchQueueController;

    QLabel *m_titleLabel = nullptr;
    QLabel *m_statusLabel = nullptr;
    QGroupBox *m_endpointGroup = nullptr;
    QComboBox *m_providerCombo = nullptr;
    QLineEdit *m_baseUrlEdit = nullptr;
    QLineEdit *m_apiKeyEdit = nullptr;
    QComboBox *m_modelCombo = nullptr;
    QPushButton *m_testLanguageDetectEndpointButton = nullptr;
    QGroupBox *m_translateEndpointGroup = nullptr;
    QComboBox *m_translateProviderCombo = nullptr;
    QLineEdit *m_translateBaseUrlEdit = nullptr;
    QLineEdit *m_translateApiKeyEdit = nullptr;
    QComboBox *m_translateModelCombo = nullptr;
    QPushButton *m_testTranslateEndpointButton = nullptr;
    QGroupBox *m_extractMcpGroup = nullptr;
    QLineEdit *m_extractMcpServerIdEdit = nullptr;
    QLineEdit *m_extractMcpToolNameEdit = nullptr;
    QPushButton *m_testExtractMcpButton = nullptr;
    QGroupBox *m_termMcpGroup = nullptr;
    QLineEdit *m_termMcpServerIdEdit = nullptr;
    QLineEdit *m_termMcpToolNameEdit = nullptr;
    QPushButton *m_testTermMcpButton = nullptr;
    QTabWidget *m_tabs = nullptr;
    QPushButton *m_detectButton = nullptr;
    QPlainTextEdit *m_inputEdit = nullptr;
    QComboBox *m_fragmentTargetLanguageCombo = nullptr;
    QLineEdit *m_fragmentInstructionsEdit = nullptr;
    QTextEdit *m_resultView = nullptr;
    QTextEdit *m_fragmentHistoryView = nullptr;
    QPushButton *m_translateFragmentButton = nullptr;
    QPushButton *m_clearFragmentHistoryButton = nullptr;
    QLineEdit *m_pdfPathEdit = nullptr;
    QPushButton *m_browsePdfButton = nullptr;
    QPushButton *m_extractPdfButton = nullptr;
    QPushButton *m_translateDocumentButton = nullptr;
    QLabel *m_documentStageLabel = nullptr;
    QProgressBar *m_documentProgressBar = nullptr;
    QTextEdit *m_pdfExtractView = nullptr;
    QTextEdit *m_translatedView = nullptr;
    QTextEdit *m_manifestView = nullptr;
    viewer::CompareReaderWidget *m_compareReader = nullptr;
    QPushButton *m_addBatchDocumentsButton = nullptr;
    QPushButton *m_pauseResumeBatchButton = nullptr;
    QPushButton *m_retryBatchButton = nullptr;
    QPushButton *m_openBatchResultButton = nullptr;
    QPushButton *m_openBatchFolderButton = nullptr;
    QPushButton *m_loadBatchCompareButton = nullptr;
    QLabel *m_batchStatusLabel = nullptr;
    QSpinBox *m_batchConcurrencySpin = nullptr;
    QTableWidget *m_batchTable = nullptr;
    QJsonArray m_fragmentHistory;
    QString m_lastFragmentTranslation;
    QString m_lastFragmentSourceLanguage;
    QString m_lastFragmentTargetLanguage;
};

} // namespace pdftranslator
