#include "models/ProjectConfig.h"
#include "services/BuildManager.h"
#include "services/LatexEnvironmentService.h"
#include "services/ProjectService.h"
#include "services/SyncTexService.h"
#include "services/TemplateService.h"

#include <QJsonDocument>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

class LaTeXAppTests final : public QObject {
    Q_OBJECT

private slots:
    void projectConfigRoundTrips()
    {
        ProjectConfig config;
        config.mainFile = QStringLiteral("paper.tex");
        config.compiler = QStringLiteral("latexmk");
        config.outputDirectory = QStringLiteral("out");
        config.autoCompile = true;
        config.synctexEnabled = false;

        const auto copy = ProjectConfig::fromJson(config.toJson());
        QCOMPARE(copy.mainFile, config.mainFile);
        QCOMPARE(copy.compiler, config.compiler);
        QCOMPARE(copy.outputDirectory, config.outputDirectory);
        QCOMPARE(copy.autoCompile, config.autoCompile);
        QCOMPARE(copy.synctexEnabled, config.synctexEnabled);
    }

    void latexmkArgumentsIncludeWindowsMvpDefaults()
    {
        ProjectConfig config;
        config.mainFile = QStringLiteral("main.tex");
        config.outputDirectory = QStringLiteral("build");
        config.synctexEnabled = true;

        const auto arguments = BuildManager::latexmkArguments(config);
        QVERIFY(arguments.contains(QStringLiteral("-pdf")));
        QVERIFY(arguments.contains(QStringLiteral("-g")));
        QVERIFY(arguments.contains(QStringLiteral("-file-line-error")));
        QVERIFY(arguments.contains(QStringLiteral("-synctex=1")));
        QVERIFY(arguments.contains(QStringLiteral("-outdir=build")));
        QCOMPARE(arguments.last(), QStringLiteral("main.tex"));
    }

    void pdflatexArgumentsIncludeFallbackDefaults()
    {
        ProjectConfig config;
        config.mainFile = QStringLiteral("main.tex");
        config.outputDirectory = QStringLiteral("build");
        config.synctexEnabled = true;

        const auto arguments = BuildManager::pdflatexArguments(config);
        QVERIFY(arguments.contains(QStringLiteral("-interaction=nonstopmode")));
        QVERIFY(arguments.contains(QStringLiteral("-file-line-error")));
        QVERIFY(arguments.contains(QStringLiteral("-halt-on-error")));
        QVERIFY(arguments.contains(QStringLiteral("-synctex=1")));
        QVERIFY(arguments.contains(QStringLiteral("-output-directory=build")));
        QCOMPARE(arguments.last(), QStringLiteral("main.tex"));
    }

    void parseFileLineDiagnostics()
    {
        const auto log = QStringLiteral("main.tex:12: Undefined control sequence.\nLaTeX Warning: Reference `x' undefined on input line 20.\n");
        const auto diagnostics = BuildManager::parseDiagnostics(log, QStringLiteral("C:/work/paper"));

        QCOMPARE(diagnostics.size(), 2);
        QVERIFY(diagnostics.at(0).severity == DiagnosticSeverity::Error);
        QCOMPARE(diagnostics.at(0).line, 12);
        QVERIFY(diagnostics.at(0).filePath.endsWith(QStringLiteral("main.tex")));
        QVERIFY(diagnostics.at(1).severity == DiagnosticSeverity::Warning);
        QCOMPARE(diagnostics.at(1).line, 20);
    }

    void parseMissingPerlDiagnostic()
    {
        const auto log = QStringLiteral("MiKTeX could not find the script engine 'perl' which is required to execute 'latexmk'.\n");
        const auto diagnostics = BuildManager::parseDiagnostics(log, QStringLiteral("C:/work/paper"));

        QCOMPARE(diagnostics.size(), 1);
        QVERIFY(diagnostics.at(0).severity == DiagnosticSeverity::Error);
        QVERIFY(diagnostics.at(0).message.contains(QStringLiteral("Perl")));
    }

    void parseStaleLatexmkStateDiagnostic()
    {
        const auto log = QStringLiteral("pdflatex: gave an error in previous invocation of latexmk.\n");
        const auto diagnostics = BuildManager::parseDiagnostics(log, QStringLiteral("C:/work/paper"));

        QCOMPARE(diagnostics.size(), 1);
        QVERIFY(diagnostics.at(0).severity == DiagnosticSeverity::Warning);
        QVERIFY(diagnostics.at(0).message.contains(QStringLiteral("stale build state")));
    }

    void parseSynctexForwardOutput()
    {
        const auto output = QStringLiteral("SyncTeX result begin\nOutput:main.pdf\nPage:2\nx:144.50\ny:233.25\nSyncTeX result end\n");
        const auto result = SyncTexService::parseForwardOutput(output);

        QVERIFY(result.success);
        QCOMPARE(result.page, 2);
        QCOMPARE(result.position.x(), 144.50);
        QCOMPARE(result.position.y(), 233.25);
    }

    void parseSynctexInverseOutput()
    {
        const auto output = QStringLiteral("SyncTeX result begin\nInput:C:/work/paper/main.tex\nLine:42\nColumn:7\nSyncTeX result end\n");
        const auto result = SyncTexService::parseInverseOutput(output);

        QVERIFY(result.success);
        QCOMPARE(result.inputFile, QStringLiteral("C:/work/paper/main.tex"));
        QCOMPARE(result.line, 42);
        QCOMPARE(result.column, 7);
    }

    void createsTemplateProject()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());

        ProjectService service;
        QString error;
        QVERIFY2(service.createProject(directory.path(), QStringLiteral("Article"), &error), qPrintable(error));
        QVERIFY(QFile::exists(QDir(directory.path()).filePath(QStringLiteral("main.tex"))));
        QVERIFY(QFile::exists(QDir(directory.path()).filePath(QStringLiteral(".latexapp/project.json"))));
    }

    void projectServiceSavesNestedMainFile()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());

        ProjectService service;
        QString error;
        QVERIFY2(service.createProject(directory.path(), QStringLiteral("Article"), &error), qPrintable(error));
        QVERIFY(QDir(directory.path()).mkpath(QStringLiteral("chapters")));

        ProjectConfig config = service.config();
        config.mainFile = QStringLiteral("chapters/intro.tex");
        service.setConfig(config);
        QVERIFY2(service.saveConfig(&error), qPrintable(error));

        ProjectService reopened;
        QVERIFY2(reopened.openPath(directory.path(), &error), qPrintable(error));
        QCOMPARE(reopened.config().mainFile, QStringLiteral("chapters/intro.tex"));
    }

    void templateServiceProvidesExpectedTemplates()
    {
        TemplateService service;
        const auto names = service.templateNames();
        QVERIFY(names.contains(QStringLiteral("Blank")));
        QVERIFY(names.contains(QStringLiteral("Article")));
        QVERIFY(names.contains(QStringLiteral("Beamer")));
        QVERIFY(service.contentForTemplate(QStringLiteral("Blank")).contains(QStringLiteral("\\begin{document}")));
        QVERIFY(service.contentForTemplate(QStringLiteral("Article")).contains(QStringLiteral("\\documentclass{article}")));
    }

    void latexEnvironmentDetectionDoesNotRequireInstalledTex()
    {
        LatexEnvironmentService service;
        const auto toolchains = service.detectToolchains();
        for (const auto &toolchain : toolchains) {
            QVERIFY(toolchain.isUsable());
        }
    }
};

QTEST_MAIN(LaTeXAppTests)

#include "main.moc"
