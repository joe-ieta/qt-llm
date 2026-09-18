#pragma once

#include <toolsstudio/toolsstudio_types.h>

#include <QDialog>

class QTextEdit;

class ToolImportPreviewDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ToolImportPreviewDialog(const qtllm::toolsstudio::ToolMergePreview &preview, QWidget *parent = nullptr);

private:
    QTextEdit *m_summaryView;
};
