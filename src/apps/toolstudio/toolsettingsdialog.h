#pragma once

#include <toolsstudio/toolstudiosettings.h>

#include <QDialog>

class QComboBox;
class QCheckBox;
class QFontComboBox;
class QSpinBox;

class ToolSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ToolSettingsDialog(qtllm::toolsstudio::ToolStudioSettings *settings, QWidget *parent = nullptr);

private slots:
    void accept() override;

private:
    qtllm::toolsstudio::ToolStudioSettings *m_settings;
    QComboBox *m_themeCombo;
    QFontComboBox *m_uiFontCombo;
    QSpinBox *m_uiFontSizeSpin;
    QFontComboBox *m_editorFontCombo;
    QSpinBox *m_editorFontSizeSpin;
    QCheckBox *m_showBuiltinCheck;
    QCheckBox *m_showDisabledCheck;
    QCheckBox *m_includeDescendantsCheck;
};
