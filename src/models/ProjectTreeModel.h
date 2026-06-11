#pragma once

#include <QSortFilterProxyModel>
#include <QString>

class ProjectTreeModel final : public QSortFilterProxyModel {
    Q_OBJECT

public:
    explicit ProjectTreeModel(QObject *parent = nullptr);

    void setProjectMetadata(const QString &projectRoot, const QString &mainFile, const QString &outputDirectory);
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    QString relativePathForSourceIndex(const QModelIndex &sourceIndex) const;

    QString m_projectRoot;
    QString m_mainFile;
    QString m_outputDirectory;
};
