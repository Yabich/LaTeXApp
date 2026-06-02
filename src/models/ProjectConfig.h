#pragma once

#include <QJsonObject>
#include <QString>

struct ProjectConfig {
    QString mainFile = QStringLiteral("main.tex");
    QString compiler = QStringLiteral("latexmk");
    QString outputDirectory = QStringLiteral("build");
    bool autoCompile = false;
    bool synctexEnabled = true;

    static ProjectConfig defaults();
    static ProjectConfig fromJson(const QJsonObject &object);
    QJsonObject toJson() const;
};

