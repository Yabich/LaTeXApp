#include "ui/MainWindow.h"

#include <QApplication>
#include <QColor>
#include <QCommandLineParser>
#include <QPalette>
#include <QStyleFactory>

namespace {
void applyLightTheme(QApplication &application)
{
    application.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    QPalette palette;
    palette.setColor(QPalette::Window, QColor(246, 247, 249));
    palette.setColor(QPalette::WindowText, QColor(20, 23, 31));
    palette.setColor(QPalette::Base, QColor(255, 255, 255));
    palette.setColor(QPalette::AlternateBase, QColor(242, 244, 247));
    palette.setColor(QPalette::ToolTipBase, QColor(255, 255, 255));
    palette.setColor(QPalette::ToolTipText, QColor(20, 23, 31));
    palette.setColor(QPalette::Text, QColor(20, 23, 31));
    palette.setColor(QPalette::Button, QColor(244, 246, 248));
    palette.setColor(QPalette::ButtonText, QColor(20, 23, 31));
    palette.setColor(QPalette::BrightText, QColor(255, 255, 255));
    palette.setColor(QPalette::Highlight, QColor(190, 215, 255));
    palette.setColor(QPalette::HighlightedText, QColor(12, 19, 32));
    palette.setColor(QPalette::Link, QColor(20, 93, 170));
    application.setPalette(palette);
}
}

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    applyLightTheme(application);
    QApplication::setOrganizationName(QStringLiteral("LaTeXApp"));
    QApplication::setApplicationName(QStringLiteral("LaTeXApp"));
    QApplication::setApplicationVersion(QStringLiteral("0.1.0"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Windows desktop LaTeX editor"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(QStringLiteral("path"), QStringLiteral("Project folder or .tex file to open."));
    parser.process(application);

    MainWindow window;
    window.show();

    const auto positional = parser.positionalArguments();
    if (!positional.isEmpty()) {
        window.openStartupPath(positional.first());
    }

    return application.exec();
}
