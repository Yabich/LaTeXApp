#pragma once

#include "models/ProjectConfig.h"

#include <QObject>
#include <QString>

class ProjectService final : public QObject {
    Q_OBJECT

public:
    explicit ProjectService(QObject *parent = nullptr);

    bool createProject(const QString &projectDirectory, const QString &templateName, QString *errorMessage = nullptr);
    bool openPath(const QString &path, QString *errorMessage = nullptr);
    bool saveConfig(QString *errorMessage = nullptr) const;

    QString projectRoot() const;
    QString mainFilePath() const;
    ProjectConfig config() const;
    void setConfig(const ProjectConfig &config);

signals:
    void projectChanged(const QString &projectRoot);

private:
    QString configPath() const;
    bool loadConfig(QString *errorMessage);

    QString m_projectRoot;
    ProjectConfig m_config;
};

