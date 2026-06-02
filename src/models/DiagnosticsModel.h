#pragma once

#include "models/Diagnostic.h"

#include <QAbstractTableModel>
#include <QVector>

class DiagnosticsModel final : public QAbstractTableModel {
    Q_OBJECT

public:
    explicit DiagnosticsModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    void setDiagnostics(QVector<Diagnostic> diagnostics);
    void clear();
    const Diagnostic &diagnosticAt(int row) const;
    QVector<Diagnostic> diagnostics() const;

private:
    QVector<Diagnostic> m_diagnostics;
};

