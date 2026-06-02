#include "widgets/LatexEditor.h"

#include <QAbstractItemView>
#include <QFile>
#include <QFontDatabase>
#include <QKeyEvent>
#include <QPainter>
#include <QScrollBar>
#include <QStringConverter>
#include <QStringListModel>
#include <QTextBlock>
#include <QTextStream>

LatexEditor::LatexEditor(QWidget *parent)
    : QPlainTextEdit(parent)
    , m_lineNumberArea(new LineNumberArea(this))
    , m_highlighter(document())
{
    const auto fixedFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    setFont(fixedFont);
    setTabStopDistance(QFontMetricsF(font()).horizontalAdvance(QLatin1Char(' ')) * 4);
    setLineWrapMode(QPlainTextEdit::NoWrap);
    setStyleSheet(QStringLiteral(
        "QPlainTextEdit {"
        "  background: #ffffff;"
        "  color: #111827;"
        "  selection-background-color: #bed7ff;"
        "  selection-color: #111827;"
        "  border: 0;"
        "}"
    ));

    m_completer = new QCompleter(completionWords(), this);
    m_completer->setWidget(this);
    m_completer->setCaseSensitivity(Qt::CaseInsensitive);
    m_completer->setCompletionMode(QCompleter::PopupCompletion);
    connect(m_completer, qOverload<const QString &>(&QCompleter::activated), this, &LatexEditor::insertCompletion);

    connect(this, &QPlainTextEdit::blockCountChanged, this, &LatexEditor::updateLineNumberAreaWidth);
    connect(this, &QPlainTextEdit::updateRequest, this, &LatexEditor::updateLineNumberArea);
    connect(this, &QPlainTextEdit::cursorPositionChanged, this, &LatexEditor::highlightCurrentLine);

    updateLineNumberAreaWidth();
    highlightCurrentLine();
}

int LatexEditor::lineNumberAreaWidth() const
{
    int digits = 1;
    int max = qMax(1, blockCount());
    while (max >= 10) {
        max /= 10;
        ++digits;
    }
    return 14 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;
}

void LatexEditor::lineNumberAreaPaintEvent(QPaintEvent *event)
{
    QPainter painter(m_lineNumberArea);
    painter.fillRect(event->rect(), QColor(244, 246, 248));

    auto block = firstVisibleBlock();
    auto blockNumber = block.blockNumber();
    auto top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    auto bottom = top + qRound(blockBoundingRect(block).height());

    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            const auto number = QString::number(blockNumber + 1);
            painter.setPen(QColor(120, 128, 138));
            painter.drawText(0, top, m_lineNumberArea->width() - 6, fontMetrics().height(), Qt::AlignRight, number);
        }

        block = block.next();
        top = bottom;
        bottom = top + qRound(blockBoundingRect(block).height());
        ++blockNumber;
    }
}

void LatexEditor::gotoLine(int line)
{
    if (line <= 0) {
        return;
    }
    QTextBlock block = document()->findBlockByNumber(line - 1);
    if (!block.isValid()) {
        return;
    }
    QTextCursor cursor(block);
    setTextCursor(cursor);
    centerCursor();
    setFocus();
}

QString LatexEditor::filePath() const
{
    return m_filePath;
}

void LatexEditor::setFilePath(const QString &filePath)
{
    m_filePath = filePath;
    document()->setModified(false);
}

bool LatexEditor::loadFromFile(const QString &filePath, QString *errorMessage)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMessage) {
            *errorMessage = file.errorString();
        }
        return false;
    }
    setPlainText(QString::fromUtf8(file.readAll()));
    setFilePath(filePath);
    return true;
}

bool LatexEditor::save(QString *errorMessage)
{
    if (m_filePath.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("No file path is associated with this editor.");
        }
        return false;
    }
    return saveAs(m_filePath, errorMessage);
}

bool LatexEditor::saveAs(const QString &filePath, QString *errorMessage)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorMessage) {
            *errorMessage = file.errorString();
        }
        return false;
    }
    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    stream << toPlainText();
    setFilePath(filePath);
    return true;
}

void LatexEditor::resizeEvent(QResizeEvent *event)
{
    QPlainTextEdit::resizeEvent(event);
    const auto cr = contentsRect();
    m_lineNumberArea->setGeometry(QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
}

void LatexEditor::keyPressEvent(QKeyEvent *event)
{
    if (m_completer && m_completer->popup()->isVisible()) {
        switch (event->key()) {
        case Qt::Key_Enter:
        case Qt::Key_Return:
        case Qt::Key_Escape:
        case Qt::Key_Tab:
        case Qt::Key_Backtab:
            event->ignore();
            return;
        default:
            break;
        }
    }

    QPlainTextEdit::keyPressEvent(event);

    if (!m_completer || event->text().isEmpty()) {
        return;
    }

    const auto prefix = wordUnderCursor();
    if (!prefix.startsWith(QLatin1Char('\\')) || prefix.size() < 2) {
        m_completer->popup()->hide();
        return;
    }

    m_completer->setCompletionPrefix(prefix);
    m_completer->popup()->setCurrentIndex(m_completer->completionModel()->index(0, 0));
    const auto rect = cursorRect();
    m_completer->complete(QRect(rect.left(), rect.bottom(), 360, rect.height()));
}

void LatexEditor::updateLineNumberAreaWidth()
{
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}

void LatexEditor::highlightCurrentLine()
{
    QList<QTextEdit::ExtraSelection> selections;
    if (!isReadOnly()) {
        QTextEdit::ExtraSelection selection;
        selection.format.setBackground(QColor(238, 244, 252));
        selection.format.setProperty(QTextFormat::FullWidthSelection, true);
        selection.cursor = textCursor();
        selection.cursor.clearSelection();
        selections.append(selection);
    }
    setExtraSelections(selections);
}

void LatexEditor::updateLineNumberArea(const QRect &rect, int dy)
{
    if (dy) {
        m_lineNumberArea->scroll(0, dy);
    } else {
        m_lineNumberArea->update(0, rect.y(), m_lineNumberArea->width(), rect.height());
    }

    if (rect.contains(viewport()->rect())) {
        updateLineNumberAreaWidth();
    }
}

void LatexEditor::insertCompletion(const QString &completion)
{
    if (!m_completer) {
        return;
    }

    auto cursor = textCursor();
    const auto extra = completion.length() - m_completer->completionPrefix().length();
    cursor.movePosition(QTextCursor::Left);
    cursor.movePosition(QTextCursor::EndOfWord);
    cursor.insertText(completion.right(extra));
    setTextCursor(cursor);
}

QString LatexEditor::wordUnderCursor() const
{
    auto cursor = textCursor();
    cursor.select(QTextCursor::WordUnderCursor);
    auto word = cursor.selectedText();
    const auto position = cursor.selectionStart();
    if (position > 0) {
        auto before = document()->characterAt(position - 1);
        if (before == QLatin1Char('\\')) {
            word.prepend(QLatin1Char('\\'));
        }
    }
    return word;
}

QStringList LatexEditor::completionWords() const
{
    return {
        QStringLiteral("\\begin{}"),
        QStringLiteral("\\end{}"),
        QStringLiteral("\\section{}"),
        QStringLiteral("\\subsection{}"),
        QStringLiteral("\\subsubsection{}"),
        QStringLiteral("\\textbf{}"),
        QStringLiteral("\\emph{}"),
        QStringLiteral("\\includegraphics{}"),
        QStringLiteral("\\label{}"),
        QStringLiteral("\\ref{}"),
        QStringLiteral("\\cite{}"),
        QStringLiteral("\\item"),
        QStringLiteral("\\frac{}{}"),
    };
}
