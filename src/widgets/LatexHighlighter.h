#pragma once

#include <QRegularExpression>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QVector>

class LatexHighlighter final : public QSyntaxHighlighter {
    Q_OBJECT

public:
    explicit LatexHighlighter(QTextDocument *parent = nullptr);

protected:
    void highlightBlock(const QString &text) override;

private:
    struct Rule {
        QRegularExpression pattern;
        QTextCharFormat format;
    };

    QVector<Rule> m_rules;
    QTextCharFormat m_commentFormat;
    QTextCharFormat m_mathFormat;
};

