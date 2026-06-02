#include "models/DiagnosticsModel.h"

#include <QColor>
#include <QFileInfo>

namespace {
QString severityToString(DiagnosticSeverity severity)
{
    switch (severity) {
    case DiagnosticSeverity::Error:
        return QStringLiteral("Error");
    case DiagnosticSeverity::Warning:
        return QStringLiteral("Warning");
    case DiagnosticSeverity::Info:
        return QStringLiteral("Info");
    }
    return QStringLiteral("Info");
}
}

DiagnosticsModel::DiagnosticsModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

int DiagnosticsModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_diagnostics.size();
}

int DiagnosticsModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : 4;
}

QVariant DiagnosticsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_diagnostics.size()) {
        return {};
    }

    const auto &diagnostic = m_diagnostics.at(index.row());

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case 0:
            return severityToString(diagnostic.severity);
        case 1:
            return QFileInfo(diagnostic.filePath).fileName();
        case 2:
            return diagnostic.line > 0 ? QVariant(diagnostic.line) : QVariant(QString());
        case 3:
            return diagnostic.message;
        default:
            return {};
        }
    }

    if (role == Qt::ToolTipRole) {
        return diagnostic.filePath;
    }

    if (role == Qt::ForegroundRole) {
        if (diagnostic.severity == DiagnosticSeverity::Error) {
            return QColor(190, 32, 32);
        }
        if (diagnostic.severity == DiagnosticSeverity::Warning) {
            return QColor(160, 110, 0);
        }
    }

    return {};
}

QVariant DiagnosticsModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
        return {};
    }

    switch (section) {
    case 0:
        return QStringLiteral("Type");
    case 1:
        return QStringLiteral("File");
    case 2:
        return QStringLiteral("Line");
    case 3:
        return QStringLiteral("Message");
    default:
        return {};
    }
}

void DiagnosticsModel::setDiagnostics(QVector<Diagnostic> diagnostics)
{
    beginResetModel();
    m_diagnostics = std::move(diagnostics);
    endResetModel();
}

void DiagnosticsModel::clear()
{
    setDiagnostics({});
}

const Diagnostic &DiagnosticsModel::diagnosticAt(int row) const
{
    return m_diagnostics.at(row);
}

QVector<Diagnostic> DiagnosticsModel::diagnostics() const
{
    return m_diagnostics;
}

