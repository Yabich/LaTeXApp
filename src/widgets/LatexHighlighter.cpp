#include "widgets/LatexHighlighter.h"

#include <QBrush>

LatexHighlighter::LatexHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
{
    QTextCharFormat commandFormat;
    commandFormat.setForeground(QColor(28, 90, 150));
    commandFormat.setFontWeight(QFont::DemiBold);

    QTextCharFormat environmentFormat;
    environmentFormat.setForeground(QColor(120, 68, 160));
    environmentFormat.setFontWeight(QFont::DemiBold);

    QTextCharFormat braceFormat;
    braceFormat.setForeground(QColor(80, 80, 80));

    m_commentFormat.setForeground(QColor(95, 125, 85));
    m_commentFormat.setFontItalic(true);

    m_mathFormat.setForeground(QColor(150, 75, 20));

    m_rules = {
        {QRegularExpression(QStringLiteral(R"(\\[A-Za-z@]+[\*]?)")), commandFormat},
        {QRegularExpression(QStringLiteral(R"(\\(begin|end)\s*\{[^}]+\})")), environmentFormat},
        {QRegularExpression(QStringLiteral(R"([\{\}\[\]])")), braceFormat},
    };
}

void LatexHighlighter::highlightBlock(const QString &text)
{
    for (const auto &rule : m_rules) {
        auto iterator = rule.pattern.globalMatch(text);
        while (iterator.hasNext()) {
            const auto match = iterator.next();
            setFormat(match.capturedStart(), match.capturedLength(), rule.format);
        }
    }

    auto commentStart = -1;
    for (int i = 0; i < text.size(); ++i) {
        if (text.at(i) == QLatin1Char('%') && (i == 0 || text.at(i - 1) != QLatin1Char('\\'))) {
            commentStart = i;
            break;
        }
    }
    if (commentStart >= 0) {
        setFormat(commentStart, text.size() - commentStart, m_commentFormat);
    }

    const QRegularExpression inlineMath(QStringLiteral(R"((\$[^\$]+\$))"));
    auto mathIterator = inlineMath.globalMatch(text);
    while (mathIterator.hasNext()) {
        const auto match = mathIterator.next();
        setFormat(match.capturedStart(), match.capturedLength(), m_mathFormat);
    }
}

