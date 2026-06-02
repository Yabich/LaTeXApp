#pragma once

#include <QObject>
#include <QString>
#include <QVector>

struct LatexToolchain {
    QString name;
    QString rootPath;
    QString latexmkPath;
    QString pdflatexPath;
    QString packageManagerPath;

    bool isUsable() const { return !latexmkPath.isEmpty() || !pdflatexPath.isEmpty(); }
};

class LatexEnvironmentService final : public QObject {
    Q_OBJECT

public:
    explicit LatexEnvironmentService(QObject *parent = nullptr);

    QVector<LatexToolchain> detectToolchains() const;
    QString configuredLatexmkPath() const;
    void setConfiguredLatexmkPath(const QString &path);
    QString preferredLatexmkPath() const;
    QString preferredPdflatexPath() const;
    bool hasPerlAvailable() const;

    static QString miktexDownloadUrl();
    static QString perlDownloadUrl();

private:
    QVector<LatexToolchain> detectFromPath() const;
    QVector<LatexToolchain> detectFromCommonDirectories() const;
    QVector<LatexToolchain> detectFromRegistry() const;
};
