#include "ui/SettingsDialog.h"

#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTextBrowser>
#include <QUrl>
#include <QVBoxLayout>

SettingsDialog::SettingsDialog(LatexEnvironmentService *environmentService, QWidget *parent)
    : QDialog(parent)
    , m_environmentService(environmentService)
{
    setWindowTitle(QStringLiteral("Preferences"));
    resize(620, 420);

    auto *layout = new QVBoxLayout(this);

    auto *formLayout = new QFormLayout();
    auto *pathRow = new QWidget(this);
    auto *pathLayout = new QHBoxLayout(pathRow);
    pathLayout->setContentsMargins(0, 0, 0, 0);
    m_latexmkPath = new QLineEdit(pathRow);
    m_latexmkPath->setText(environmentService->configuredLatexmkPath());
    auto *browseButton = new QPushButton(QStringLiteral("Browse"), pathRow);
    connect(browseButton, &QPushButton::clicked, this, &SettingsDialog::browseLatexmk);
    pathLayout->addWidget(m_latexmkPath);
    pathLayout->addWidget(browseButton);
    formLayout->addRow(QStringLiteral("latexmk path"), pathRow);
    layout->addLayout(formLayout);

    layout->addWidget(new QLabel(QStringLiteral("Detected Windows TeX installations"), this));
    m_detectedToolchains = new QTextBrowser(this);
    m_detectedToolchains->setOpenExternalLinks(true);
    layout->addWidget(m_detectedToolchains);

    auto *miktexButton = new QPushButton(QStringLiteral("Open MiKTeX Download"), this);
    connect(miktexButton, &QPushButton::clicked, this, []() {
        QDesktopServices::openUrl(QUrl(LatexEnvironmentService::miktexDownloadUrl()));
    });
    layout->addWidget(miktexButton);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &SettingsDialog::saveSettings);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    refreshDetectedToolchains();
}

void SettingsDialog::browseLatexmk()
{
    const auto path = QFileDialog::getOpenFileName(this, QStringLiteral("Select latexmk.exe"), {}, QStringLiteral("latexmk (latexmk.exe);;Executables (*.exe)"));
    if (!path.isEmpty()) {
        m_latexmkPath->setText(path);
    }
}

void SettingsDialog::saveSettings()
{
    m_environmentService->setConfiguredLatexmkPath(m_latexmkPath->text().trimmed());
    accept();
}

void SettingsDialog::refreshDetectedToolchains()
{
    const auto toolchains = m_environmentService->detectToolchains();
    QString html;
    if (toolchains.isEmpty()) {
        html = QStringLiteral("<p>No TeX installation was detected. Install MiKTeX or configure latexmk manually.</p>");
    } else {
        html = QStringLiteral("<ul>");
        for (const auto &toolchain : toolchains) {
            html += QStringLiteral("<li><b>%1</b><br>latexmk: %2<br>pdflatex: %3</li>")
                        .arg(toolchain.name.toHtmlEscaped(),
                             toolchain.latexmkPath.toHtmlEscaped(),
                             toolchain.pdflatexPath.toHtmlEscaped());
        }
        html += QStringLiteral("</ul>");
    }
    m_detectedToolchains->setHtml(html);
}

