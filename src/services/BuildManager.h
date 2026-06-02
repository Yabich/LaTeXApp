#pragma once

#include "models/Diagnostic.h"
#include "models/ProjectConfig.h"

#include <QObject>
#include <QProcess>
#include <QVector>

enum class BuildEngine {
    Latexmk,
    PdfLatex
};

class BuildManager final : public QObject {
    Q_OBJECT

public:
    explicit BuildManager(QObject *parent = nullptr);

    bool isRunning() const;
    void build(const QString &projectRoot, const ProjectConfig &config, const QString &compilerPath, BuildEngine engine);
    void cancel();

    static QVector<Diagnostic> parseDiagnostics(const QString &log, const QString &projectRoot);
    static QStringList latexmkArguments(const ProjectConfig &config);
    static QStringList pdflatexArguments(const ProjectConfig &config);
    static QStringList compilerArguments(const ProjectConfig &config, BuildEngine engine);

signals:
    void buildStarted();
    void buildOutput(const QString &text);
    void buildFinished(bool success, const QString &pdfPath, QVector<Diagnostic> diagnostics);

private:
    QProcess m_process;
    QString m_projectRoot;
    ProjectConfig m_config;
    BuildEngine m_engine = BuildEngine::Latexmk;
    QString m_output;
};
