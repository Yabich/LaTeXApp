#include "services/BuildManager.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>

BuildManager::BuildManager(QObject *parent)
    : QObject(parent)
{
    connect(&m_process, &QProcess::readyReadStandardOutput, this, [this]() {
        const auto text = QString::fromLocal8Bit(m_process.readAllStandardOutput());
        m_output += text;
        emit buildOutput(text);
    });
    connect(&m_process, &QProcess::readyReadStandardError, this, [this]() {
        const auto text = QString::fromLocal8Bit(m_process.readAllStandardError());
        m_output += text;
        emit buildOutput(text);
    });
    connect(&m_process, &QProcess::finished, this, [this](int exitCode, QProcess::ExitStatus exitStatus) {
        const auto diagnostics = parseDiagnostics(m_output, m_projectRoot);
        const auto mainBaseName = QFileInfo(m_config.mainFile).completeBaseName();
        const auto pdfPath = QDir(m_projectRoot).filePath(QDir(m_config.outputDirectory).filePath(mainBaseName + QStringLiteral(".pdf")));
        emit buildFinished(exitStatus == QProcess::NormalExit && exitCode == 0, pdfPath, diagnostics);
    });
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            m_output += m_process.errorString();
            emit buildOutput(m_process.errorString());
            Diagnostic diagnostic;
            diagnostic.severity = DiagnosticSeverity::Error;
            diagnostic.message = m_process.errorString();
            emit buildFinished(false, QString(), {diagnostic});
        }
    });
}

bool BuildManager::isRunning() const
{
    return m_process.state() != QProcess::NotRunning;
}

void BuildManager::build(const QString &projectRoot, const ProjectConfig &config, const QString &compilerPath, BuildEngine engine)
{
    if (isRunning()) {
        cancel();
    }

    m_projectRoot = projectRoot;
    m_config = config;
    m_engine = engine;
    m_output.clear();

    QDir(projectRoot).mkpath(config.outputDirectory);
    m_process.setWorkingDirectory(projectRoot);
    m_process.setProgram(compilerPath);
    m_process.setArguments(compilerArguments(config, engine));
    emit buildStarted();
    m_process.start();
}

void BuildManager::cancel()
{
    if (!isRunning()) {
        return;
    }
    m_process.kill();
    m_process.waitForFinished(3000);
}

QVector<Diagnostic> BuildManager::parseDiagnostics(const QString &log, const QString &projectRoot)
{
    QVector<Diagnostic> diagnostics;
    const auto lines = log.split(QRegularExpression(QStringLiteral("\\r?\\n")));
    const QRegularExpression fileLineError(QStringLiteral(R"((.+?\.tex):(\d+):\s*(.+))"));
    const QRegularExpression latexWarning(QStringLiteral(R"(LaTeX Warning:\s*(.+?)(?: on input line (\d+))?\.?$)"));
    QString currentFile;

    for (const auto &line : lines) {
        if (line.contains(QStringLiteral("could not find the script engine 'perl'"), Qt::CaseInsensitive)
            || line.contains(QStringLiteral("Make sure 'perl' is installed"), Qt::CaseInsensitive)) {
            Diagnostic diagnostic;
            diagnostic.severity = DiagnosticSeverity::Error;
            diagnostic.message = QStringLiteral("MiKTeX latexmk requires Perl. Install Strawberry Perl or use the pdflatex fallback.");
            diagnostics.append(diagnostic);
            continue;
        }

        if (line.contains(QStringLiteral("gave an error in previous invocation of latexmk"), Qt::CaseInsensitive)
            || line.contains(QStringLiteral("clean out generated files"), Qt::CaseInsensitive)) {
            Diagnostic diagnostic;
            diagnostic.severity = DiagnosticSeverity::Warning;
            diagnostic.message = QStringLiteral("latexmk is reporting stale build state from an earlier failed run. Clean generated files if this persists.");
            diagnostics.append(diagnostic);
            continue;
        }

        const auto errorMatch = fileLineError.match(line);
        if (errorMatch.hasMatch()) {
            Diagnostic diagnostic;
            diagnostic.severity = line.contains(QStringLiteral("warning"), Qt::CaseInsensitive)
                ? DiagnosticSeverity::Warning
                : DiagnosticSeverity::Error;
            diagnostic.filePath = QDir(projectRoot).filePath(errorMatch.captured(1));
            diagnostic.line = errorMatch.captured(2).toInt();
            diagnostic.message = errorMatch.captured(3).trimmed();
            diagnostics.append(diagnostic);
            currentFile = diagnostic.filePath;
            continue;
        }

        if (line.startsWith(QStringLiteral("! "))) {
            Diagnostic diagnostic;
            diagnostic.severity = DiagnosticSeverity::Error;
            diagnostic.filePath = currentFile;
            diagnostic.message = line.mid(2).trimmed();
            diagnostics.append(diagnostic);
            continue;
        }

        const auto warningMatch = latexWarning.match(line);
        if (warningMatch.hasMatch()) {
            Diagnostic diagnostic;
            diagnostic.severity = DiagnosticSeverity::Warning;
            diagnostic.filePath = currentFile;
            diagnostic.line = warningMatch.captured(2).toInt();
            diagnostic.message = warningMatch.captured(1).trimmed();
            diagnostics.append(diagnostic);
        }
    }

    return diagnostics;
}

QStringList BuildManager::latexmkArguments(const ProjectConfig &config)
{
    QStringList arguments = {
        QStringLiteral("-pdf"),
        QStringLiteral("-g"),
        QStringLiteral("-interaction=nonstopmode"),
        QStringLiteral("-file-line-error"),
        QStringLiteral("-outdir=%1").arg(config.outputDirectory),
    };

    if (config.synctexEnabled) {
        arguments.append(QStringLiteral("-synctex=1"));
    }

    arguments.append(config.mainFile);
    return arguments;
}

QStringList BuildManager::pdflatexArguments(const ProjectConfig &config)
{
    QStringList arguments = {
        QStringLiteral("-interaction=nonstopmode"),
        QStringLiteral("-file-line-error"),
        QStringLiteral("-halt-on-error"),
        QStringLiteral("-output-directory=%1").arg(config.outputDirectory),
    };

    if (config.synctexEnabled) {
        arguments.append(QStringLiteral("-synctex=1"));
    }

    arguments.append(config.mainFile);
    return arguments;
}

QStringList BuildManager::compilerArguments(const ProjectConfig &config, BuildEngine engine)
{
    return engine == BuildEngine::Latexmk ? latexmkArguments(config) : pdflatexArguments(config);
}
