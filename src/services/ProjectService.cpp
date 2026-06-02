#include "services/ProjectService.h"

#include "services/TemplateService.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QTextStream>

ProjectService::ProjectService(QObject *parent)
    : QObject(parent)
{
}

bool ProjectService::createProject(const QString &projectDirectory, const QString &templateName, QString *errorMessage)
{
    QDir directory(projectDirectory);
    if (!directory.exists() && !directory.mkpath(QStringLiteral("."))) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Could not create project directory.");
        }
        return false;
    }

    if (!directory.mkpath(QStringLiteral(".latexapp"))) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Could not create .latexapp metadata directory.");
        }
        return false;
    }

    TemplateService templates;
    QFile mainFile(directory.filePath(QStringLiteral("main.tex")));
    if (!mainFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorMessage) {
            *errorMessage = mainFile.errorString();
        }
        return false;
    }
    QTextStream stream(&mainFile);
    stream << templates.contentForTemplate(templateName);

    m_projectRoot = directory.absolutePath();
    m_config = ProjectConfig::defaults();
    if (!saveConfig(errorMessage)) {
        return false;
    }

    emit projectChanged(m_projectRoot);
    return true;
}

bool ProjectService::openPath(const QString &path, QString *errorMessage)
{
    const QFileInfo info(path);
    if (!info.exists()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("The selected path does not exist.");
        }
        return false;
    }

    if (info.isDir()) {
        m_projectRoot = info.absoluteFilePath();
        if (!loadConfig(errorMessage)) {
            m_config = ProjectConfig::defaults();
            const QDir directory(m_projectRoot);
            const auto texFiles = directory.entryList({QStringLiteral("*.tex")}, QDir::Files);
            if (!texFiles.isEmpty()) {
                m_config.mainFile = texFiles.first();
            }
        }
    } else {
        m_projectRoot = info.absolutePath();
        m_config = ProjectConfig::defaults();
        m_config.mainFile = info.fileName();
        loadConfig(nullptr);
    }

    emit projectChanged(m_projectRoot);
    return true;
}

bool ProjectService::saveConfig(QString *errorMessage) const
{
    if (m_projectRoot.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("No project is open.");
        }
        return false;
    }

    QDir directory(m_projectRoot);
    if (!directory.mkpath(QStringLiteral(".latexapp"))) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Could not create project metadata directory.");
        }
        return false;
    }

    QFile file(configPath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorMessage) {
            *errorMessage = file.errorString();
        }
        return false;
    }

    file.write(QJsonDocument(m_config.toJson()).toJson(QJsonDocument::Indented));
    return true;
}

QString ProjectService::projectRoot() const
{
    return m_projectRoot;
}

QString ProjectService::mainFilePath() const
{
    if (m_projectRoot.isEmpty()) {
        return {};
    }
    return QDir(m_projectRoot).filePath(m_config.mainFile);
}

ProjectConfig ProjectService::config() const
{
    return m_config;
}

void ProjectService::setConfig(const ProjectConfig &config)
{
    m_config = config;
}

QString ProjectService::configPath() const
{
    return QDir(m_projectRoot).filePath(QStringLiteral(".latexapp/project.json"));
}

bool ProjectService::loadConfig(QString *errorMessage)
{
    QFile file(configPath());
    if (!file.exists()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Project config was not found; defaults will be used.");
        }
        return false;
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMessage) {
            *errorMessage = file.errorString();
        }
        return false;
    }

    const auto document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Project config is not valid JSON.");
        }
        return false;
    }

    m_config = ProjectConfig::fromJson(document.object());
    return true;
}

