#pragma once

#include <QString>
#include <QStringList>

class TemplateService final {
public:
    QStringList templateNames() const;
    QString contentForTemplate(const QString &templateName) const;
};

