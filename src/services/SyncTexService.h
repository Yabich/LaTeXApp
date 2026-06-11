#pragma once

#include <QPointF>
#include <QString>

struct SyncTexForwardResult {
    bool success = false;
    int page = 0;
    QPointF position;
    QString message;
};

struct SyncTexInverseResult {
    bool success = false;
    QString inputFile;
    int line = 0;
    int column = 0;
    QString message;
};

class SyncTexService final {
public:
    QString synctexPath() const;

    SyncTexForwardResult forwardSearch(const QString &texPath, int line, int column, const QString &pdfPath) const;
    SyncTexInverseResult inverseSearch(const QString &pdfPath, int oneBasedPage, const QPointF &pagePoint) const;

    static SyncTexForwardResult parseForwardOutput(const QString &output);
    static SyncTexInverseResult parseInverseOutput(const QString &output);

private:
    QString executableIn(const QString &directory) const;
};
