#include "models/ProjectConfig.h"

ProjectConfig ProjectConfig::defaults()
{
    return {};
}

ProjectConfig ProjectConfig::fromJson(const QJsonObject &object)
{
    auto config = defaults();
    config.mainFile = object.value(QStringLiteral("mainFile")).toString(config.mainFile);
    config.compiler = object.value(QStringLiteral("compiler")).toString(config.compiler);
    config.outputDirectory = object.value(QStringLiteral("outputDirectory")).toString(config.outputDirectory);
    config.autoCompile = object.value(QStringLiteral("autoCompile")).toBool(config.autoCompile);
    config.synctexEnabled = object.value(QStringLiteral("synctexEnabled")).toBool(config.synctexEnabled);
    return config;
}

QJsonObject ProjectConfig::toJson() const
{
    return {
        {QStringLiteral("mainFile"), mainFile},
        {QStringLiteral("compiler"), compiler},
        {QStringLiteral("outputDirectory"), outputDirectory},
        {QStringLiteral("autoCompile"), autoCompile},
        {QStringLiteral("synctexEnabled"), synctexEnabled},
    };
}

