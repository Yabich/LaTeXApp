#pragma once

#include "services/LatexEnvironmentService.h"

#include <QDialog>

class QLineEdit;
class QTextBrowser;

class SettingsDialog final : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(LatexEnvironmentService *environmentService, QWidget *parent = nullptr);

private:
    void browseLatexmk();
    void saveSettings();
    void refreshDetectedToolchains();

    LatexEnvironmentService *m_environmentService = nullptr;
    QLineEdit *m_latexmkPath = nullptr;
    QTextBrowser *m_detectedToolchains = nullptr;
};

