#include "models/ProjectTreeModel.h"

#include <QDir>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QFont>

ProjectTreeModel::ProjectTreeModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    setDynamicSortFilter(true);
    setRecursiveFilteringEnabled(true);
}

void ProjectTreeModel::setProjectMetadata(const QString &projectRoot, const QString &mainFile, const QString &outputDirectory)
{
    m_projectRoot = QFileInfo(projectRoot).absoluteFilePath();
    m_mainFile = QDir::cleanPath(mainFile);
    m_outputDirectory = QDir::cleanPath(outputDirectory);
    invalidateFilter();
}

QVariant ProjectTreeModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return {};
    }

    const auto sourceIndex = mapToSource(index);
    if (index.column() == 0 && role == Qt::DisplayRole && relativePathForSourceIndex(sourceIndex) == m_mainFile) {
        return QStringLiteral("%1 [main]").arg(QSortFilterProxyModel::data(index, role).toString());
    }

    if (index.column() == 0 && role == Qt::FontRole && relativePathForSourceIndex(sourceIndex) == m_mainFile) {
        auto font = QSortFilterProxyModel::data(index, role).value<QFont>();
        font.setBold(true);
        return font;
    }

    return QSortFilterProxyModel::data(index, role);
}

bool ProjectTreeModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    const auto sourceIndex = sourceModel()->index(sourceRow, 0, sourceParent);
    const auto *fileModel = qobject_cast<QFileSystemModel *>(sourceModel());
    if (!fileModel) {
        return QSortFilterProxyModel::filterAcceptsRow(sourceRow, sourceParent);
    }

    const QFileInfo info = fileModel->fileInfo(sourceIndex);
    if (!info.exists()) {
        return false;
    }

    if (info.isDir()) {
        const auto name = info.fileName();
        if (name == QStringLiteral(".latexapp")) {
            return false;
        }

        const auto relativePath = relativePathForSourceIndex(sourceIndex);
        if (!m_outputDirectory.isEmpty()
            && m_outputDirectory != QStringLiteral(".")
            && relativePath == m_outputDirectory) {
            return false;
        }
    }

    return QSortFilterProxyModel::filterAcceptsRow(sourceRow, sourceParent);
}

QString ProjectTreeModel::relativePathForSourceIndex(const QModelIndex &sourceIndex) const
{
    const auto *fileModel = qobject_cast<QFileSystemModel *>(sourceModel());
    if (!fileModel || m_projectRoot.isEmpty()) {
        return {};
    }

    const QFileInfo info = fileModel->fileInfo(sourceIndex);
    const auto absolutePath = info.exists() ? info.canonicalFilePath() : info.absoluteFilePath();
    return QDir::cleanPath(QDir(m_projectRoot).relativeFilePath(absolutePath));
}
