#include "services/SyncTexService.h"

#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>

namespace {
QString firstCapture(const QString &text, const QString &pattern)
{
    const QRegularExpression expression(pattern, QRegularExpression::MultilineOption);
    const auto match = expression.match(text);
    return match.hasMatch() ? match.captured(1).trimmed() : QString();
}

double firstDouble(const QString &text, const QString &pattern, double fallback = 0.0)
{
    bool ok = false;
    const auto value = firstCapture(text, pattern).toDouble(&ok);
    return ok ? value : fallback;
}

int firstInt(const QString &text, const QString &pattern, int fallback = 0)
{
    bool ok = false;
    const auto value = firstCapture(text, pattern).toInt(&ok);
    return ok ? value : fallback;
}

QString processText(QProcess &process)
{
    return QString::fromLocal8Bit(process.readAllStandardOutput())
        + QString::fromLocal8Bit(process.readAllStandardError());
}
}

QString SyncTexService::synctexPath() const
{
    const auto fromPath = QStandardPaths::findExecutable(QStringLiteral("synctex"));
    if (!fromPath.isEmpty()) {
        return fromPath;
    }

    const QStringList directories = {
        QStringLiteral("C:/Program Files/MiKTeX/miktex/bin/x64"),
        QStringLiteral("C:/Program Files/MiKTeX 2.9/miktex/bin/x64"),
        QStringLiteral("C:/texlive/2026/bin/windows"),
        QStringLiteral("C:/texlive/2025/bin/windows"),
        QDir::home().filePath(QStringLiteral("AppData/Roaming/TinyTeX/bin/windows")),
        QDir::home().filePath(QStringLiteral("AppData/Local/Programs/MiKTeX/miktex/bin/x64")),
    };

    for (const auto &directory : directories) {
        const auto candidate = executableIn(directory);
        if (!candidate.isEmpty()) {
            return candidate;
        }
    }

    return {};
}

SyncTexForwardResult SyncTexService::forwardSearch(const QString &texPath, int line, int column, const QString &pdfPath) const
{
    const auto executable = synctexPath();
    if (executable.isEmpty()) {
        return {false, 0, {}, QStringLiteral("synctex.exe was not found.")};
    }
    if (!QFileInfo::exists(pdfPath)) {
        return {false, 0, {}, QStringLiteral("Compile the document before using source/PDF sync.")};
    }

    QProcess process;
    process.setProgram(executable);
    process.setArguments({
        QStringLiteral("view"),
        QStringLiteral("-i"),
        QStringLiteral("%1:%2:%3").arg(line).arg(qMax(0, column)).arg(QDir::toNativeSeparators(texPath)),
        QStringLiteral("-o"),
        QDir::toNativeSeparators(pdfPath),
    });
    process.start();
    if (!process.waitForStarted(3000)) {
        return {false, 0, {}, process.errorString()};
    }
    process.waitForFinished(10000);

    auto result = parseForwardOutput(processText(process));
    if (!result.success && result.message.isEmpty()) {
        result.message = QStringLiteral("SyncTeX did not find a matching PDF position.");
    }
    return result;
}

SyncTexInverseResult SyncTexService::inverseSearch(const QString &pdfPath, int oneBasedPage, const QPointF &pagePoint) const
{
    const auto executable = synctexPath();
    if (executable.isEmpty()) {
        return {false, {}, 0, 0, QStringLiteral("synctex.exe was not found.")};
    }
    if (!QFileInfo::exists(pdfPath)) {
        return {false, {}, 0, 0, QStringLiteral("Compile the document before using PDF/source sync.")};
    }

    QProcess process;
    process.setProgram(executable);
    process.setArguments({
        QStringLiteral("edit"),
        QStringLiteral("-o"),
        QStringLiteral("%1:%2:%3:%4")
            .arg(qMax(1, oneBasedPage))
            .arg(pagePoint.x(), 0, 'f', 2)
            .arg(pagePoint.y(), 0, 'f', 2)
            .arg(QDir::toNativeSeparators(pdfPath)),
    });
    process.start();
    if (!process.waitForStarted(3000)) {
        return {false, {}, 0, 0, process.errorString()};
    }
    process.waitForFinished(10000);

    auto result = parseInverseOutput(processText(process));
    if (!result.success && result.message.isEmpty()) {
        result.message = QStringLiteral("SyncTeX did not find a matching source position.");
    }
    return result;
}

SyncTexForwardResult SyncTexService::parseForwardOutput(const QString &output)
{
    SyncTexForwardResult result;
    result.page = firstInt(output, QStringLiteral(R"(^\s*Page:\s*([0-9]+)\s*$)"));
    result.position = QPointF(
        firstDouble(output, QStringLiteral(R"(^\s*x:\s*([-+0-9.]+)\s*$)")),
        firstDouble(output, QStringLiteral(R"(^\s*y:\s*([-+0-9.]+)\s*$)")));
    result.message = firstCapture(output, QStringLiteral(R"(^\s*(?:SyncTeX|synctex)\s+(?:warning|error):\s*(.+)$)"));
    result.success = result.page > 0;
    return result;
}

SyncTexInverseResult SyncTexService::parseInverseOutput(const QString &output)
{
    SyncTexInverseResult result;
    result.inputFile = firstCapture(output, QStringLiteral(R"(^\s*Input:\s*(.+)\s*$)"));
    result.line = firstInt(output, QStringLiteral(R"(^\s*Line:\s*([0-9]+)\s*$)"));
    result.column = firstInt(output, QStringLiteral(R"(^\s*Column:\s*([0-9]+)\s*$)"));
    result.message = firstCapture(output, QStringLiteral(R"(^\s*(?:SyncTeX|synctex)\s+(?:warning|error):\s*(.+)$)"));
    result.success = !result.inputFile.isEmpty() && result.line > 0;
    return result;
}

QString SyncTexService::executableIn(const QString &directory) const
{
    const auto path = QDir(directory).filePath(QStringLiteral("synctex.exe"));
    return QFileInfo::exists(path) ? QDir::toNativeSeparators(path) : QString();
}
