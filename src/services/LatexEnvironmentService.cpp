#include "services/LatexEnvironmentService.h"

#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>

namespace {
QString executableIn(const QString &directory, const QString &name)
{
    const auto path = QDir(directory).filePath(name + QStringLiteral(".exe"));
    return QFileInfo::exists(path) ? QDir::toNativeSeparators(path) : QString();
}

void appendUnique(QVector<LatexToolchain> &toolchains, const LatexToolchain &candidate)
{
    if (!candidate.isUsable()) {
        return;
    }

    for (const auto &existing : std::as_const(toolchains)) {
        if ((!candidate.latexmkPath.isEmpty() && existing.latexmkPath.compare(candidate.latexmkPath, Qt::CaseInsensitive) == 0)
            || (!candidate.pdflatexPath.isEmpty() && existing.pdflatexPath.compare(candidate.pdflatexPath, Qt::CaseInsensitive) == 0)) {
            return;
        }
    }

    toolchains.append(candidate);
}
}

LatexEnvironmentService::LatexEnvironmentService(QObject *parent)
    : QObject(parent)
{
}

QVector<LatexToolchain> LatexEnvironmentService::detectToolchains() const
{
    QVector<LatexToolchain> toolchains;
    for (const auto &toolchain : detectFromPath()) {
        appendUnique(toolchains, toolchain);
    }
    for (const auto &toolchain : detectFromCommonDirectories()) {
        appendUnique(toolchains, toolchain);
    }
    for (const auto &toolchain : detectFromRegistry()) {
        appendUnique(toolchains, toolchain);
    }
    return toolchains;
}

QString LatexEnvironmentService::configuredLatexmkPath() const
{
    QSettings settings;
    return settings.value(QStringLiteral("latex/latexmkPath")).toString();
}

void LatexEnvironmentService::setConfiguredLatexmkPath(const QString &path)
{
    QSettings settings;
    settings.setValue(QStringLiteral("latex/latexmkPath"), path);
}

QString LatexEnvironmentService::preferredLatexmkPath() const
{
    const auto configured = configuredLatexmkPath();
    if (!configured.isEmpty() && QFileInfo::exists(configured)) {
        return configured;
    }

    const auto toolchains = detectToolchains();
    for (const auto &toolchain : toolchains) {
        if (!toolchain.latexmkPath.isEmpty()) {
            return toolchain.latexmkPath;
        }
    }

    return QStandardPaths::findExecutable(QStringLiteral("latexmk"));
}

QString LatexEnvironmentService::preferredPdflatexPath() const
{
    const auto toolchains = detectToolchains();
    for (const auto &toolchain : toolchains) {
        if (!toolchain.pdflatexPath.isEmpty()) {
            return toolchain.pdflatexPath;
        }
    }

    return QStandardPaths::findExecutable(QStringLiteral("pdflatex"));
}

bool LatexEnvironmentService::hasPerlAvailable() const
{
    if (!QStandardPaths::findExecutable(QStringLiteral("perl")).isEmpty()) {
        return true;
    }

    const QStringList commonPerlPaths = {
        QStringLiteral("C:/Strawberry/perl/bin/perl.exe"),
        QStringLiteral("C:/Perl64/bin/perl.exe"),
        QStringLiteral("C:/Program Files/Git/usr/bin/perl.exe"),
    };

    for (const auto &path : commonPerlPaths) {
        if (QFileInfo::exists(path)) {
            return true;
        }
    }

    return false;
}

QString LatexEnvironmentService::miktexDownloadUrl()
{
    return QStringLiteral("https://miktex.org/download");
}

QString LatexEnvironmentService::perlDownloadUrl()
{
    return QStringLiteral("https://strawberryperl.com/");
}

QVector<LatexToolchain> LatexEnvironmentService::detectFromPath() const
{
    LatexToolchain toolchain;
    toolchain.name = QStringLiteral("PATH");
    toolchain.latexmkPath = QStandardPaths::findExecutable(QStringLiteral("latexmk"));
    toolchain.pdflatexPath = QStandardPaths::findExecutable(QStringLiteral("pdflatex"));
    toolchain.packageManagerPath = QStandardPaths::findExecutable(QStringLiteral("mpm"));
    return toolchain.isUsable() ? QVector<LatexToolchain>{toolchain} : QVector<LatexToolchain>{};
}

QVector<LatexToolchain> LatexEnvironmentService::detectFromCommonDirectories() const
{
    const QStringList roots = {
        QStringLiteral("C:/Program Files/MiKTeX/miktex/bin/x64"),
        QStringLiteral("C:/Program Files/MiKTeX 2.9/miktex/bin/x64"),
        QStringLiteral("C:/texlive/2026/bin/windows"),
        QStringLiteral("C:/texlive/2025/bin/windows"),
        QDir::home().filePath(QStringLiteral("AppData/Roaming/TinyTeX/bin/windows")),
        QDir::home().filePath(QStringLiteral("AppData/Local/Programs/MiKTeX/miktex/bin/x64")),
    };

    QVector<LatexToolchain> toolchains;
    for (const auto &root : roots) {
        LatexToolchain toolchain;
        toolchain.rootPath = QDir::toNativeSeparators(root);
        toolchain.name = root.contains(QStringLiteral("MiKTeX"), Qt::CaseInsensitive) ? QStringLiteral("MiKTeX")
            : root.contains(QStringLiteral("TinyTeX"), Qt::CaseInsensitive) ? QStringLiteral("TinyTeX")
                                                                             : QStringLiteral("TeX Live");
        toolchain.latexmkPath = executableIn(root, QStringLiteral("latexmk"));
        toolchain.pdflatexPath = executableIn(root, QStringLiteral("pdflatex"));
        toolchain.packageManagerPath = executableIn(root, toolchain.name == QStringLiteral("MiKTeX") ? QStringLiteral("mpm") : QStringLiteral("tlmgr"));
        appendUnique(toolchains, toolchain);
    }
    return toolchains;
}

QVector<LatexToolchain> LatexEnvironmentService::detectFromRegistry() const
{
    QVector<LatexToolchain> toolchains;
    const QStringList keys = {
        QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\MiKTeX.org\\MiKTeX"),
        QStringLiteral("HKEY_CURRENT_USER\\SOFTWARE\\MiKTeX.org\\MiKTeX"),
        QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\MiKTeX"),
        QStringLiteral("HKEY_CURRENT_USER\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\MiKTeX"),
    };

    for (const auto &key : keys) {
        QSettings registry(key, QSettings::NativeFormat);
        const auto installRoot = registry.value(QStringLiteral("InstallRoot")).toString();
        const auto installLocation = registry.value(QStringLiteral("InstallLocation")).toString();
        const auto root = !installRoot.isEmpty() ? installRoot : installLocation;
        if (root.isEmpty()) {
            continue;
        }

        const QStringList binCandidates = {
            QDir(root).filePath(QStringLiteral("miktex/bin/x64")),
            QDir(root).filePath(QStringLiteral("bin/x64")),
        };

        for (const auto &bin : binCandidates) {
            LatexToolchain toolchain;
            toolchain.name = QStringLiteral("MiKTeX");
            toolchain.rootPath = QDir::toNativeSeparators(root);
            toolchain.latexmkPath = executableIn(bin, QStringLiteral("latexmk"));
            toolchain.pdflatexPath = executableIn(bin, QStringLiteral("pdflatex"));
            toolchain.packageManagerPath = executableIn(bin, QStringLiteral("mpm"));
            appendUnique(toolchains, toolchain);
        }
    }

    return toolchains;
}
