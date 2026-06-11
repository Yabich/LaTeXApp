#pragma once

#include "widgets/LatexHighlighter.h"

#include <QCompleter>
#include <QPlainTextEdit>
#include <QPointer>

class LineNumberArea;

class LatexEditor final : public QPlainTextEdit {
    Q_OBJECT

public:
    explicit LatexEditor(QWidget *parent = nullptr);

    int lineNumberAreaWidth() const;
    void lineNumberAreaPaintEvent(QPaintEvent *event);
    void gotoLine(int line);
    QString filePath() const;
    void setFilePath(const QString &filePath);
    bool loadFromFile(const QString &filePath, QString *errorMessage = nullptr);
    bool save(QString *errorMessage = nullptr);
    bool saveAs(const QString &filePath, QString *errorMessage = nullptr);

protected:
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

signals:
    void sourceSyncRequested(const QString &filePath, int line, int column);

private slots:
    void updateLineNumberAreaWidth();
    void highlightCurrentLine();
    void updateLineNumberArea(const QRect &rect, int dy);
    void insertCompletion(const QString &completion);

private:
    QString wordUnderCursor() const;
    QStringList completionWords() const;

    QWidget *m_lineNumberArea = nullptr;
    LatexHighlighter m_highlighter;
    QPointer<QCompleter> m_completer;
    QString m_filePath;
};

class LineNumberArea final : public QWidget {
public:
    explicit LineNumberArea(LatexEditor *editor)
        : QWidget(editor)
        , m_editor(editor)
    {
    }

    QSize sizeHint() const override { return QSize(m_editor->lineNumberAreaWidth(), 0); }

protected:
    void paintEvent(QPaintEvent *event) override { m_editor->lineNumberAreaPaintEvent(event); }

private:
    LatexEditor *m_editor = nullptr;
};
