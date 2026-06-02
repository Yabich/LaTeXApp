#include "services/TemplateService.h"

#include <QMap>

namespace {
QMap<QString, QString> templates()
{
    return {
        {QStringLiteral("Article"), QStringLiteral(R"(\documentclass{article}

\usepackage[utf8]{inputenc}
\usepackage[T1]{fontenc}
\usepackage{amsmath}
\usepackage{graphicx}
\usepackage{hyperref}

\title{New Article}
\author{Author}
\date{\today}

\begin{document}

\maketitle

\section{Introduction}

Start writing here.

\end{document}
)")},
        {QStringLiteral("Report"), QStringLiteral(R"(\documentclass{report}

\usepackage[utf8]{inputenc}
\usepackage[T1]{fontenc}
\usepackage{amsmath}
\usepackage{graphicx}
\usepackage{hyperref}

\title{New Report}
\author{Author}
\date{\today}

\begin{document}

\maketitle
\tableofcontents

\chapter{Introduction}

Start writing here.

\end{document}
)")},
        {QStringLiteral("Thesis"), QStringLiteral(R"(\documentclass[12pt]{report}

\usepackage[utf8]{inputenc}
\usepackage[T1]{fontenc}
\usepackage{amsmath}
\usepackage{graphicx}
\usepackage{hyperref}

\title{Thesis Title}
\author{Author}
\date{\today}

\begin{document}

\maketitle
\tableofcontents

\chapter{Introduction}

State the research problem here.

\chapter{Method}

Describe the method here.

\chapter{Results}

Present results here.

\end{document}
)")},
        {QStringLiteral("Beamer"), QStringLiteral(R"(\documentclass{beamer}

\usetheme{Madrid}

\title{Presentation Title}
\author{Author}
\date{\today}

\begin{document}

\begin{frame}
    \titlepage
\end{frame}

\begin{frame}{Overview}
    \begin{itemize}
        \item First point
        \item Second point
    \end{itemize}
\end{frame}

\end{document}
)")},
        {QStringLiteral("Letter"), QStringLiteral(R"(\documentclass{letter}

\signature{Author}
\address{Street Address \\ City, State}

\begin{document}

\begin{letter}{Recipient \\ Address}
\opening{Dear Recipient,}

Write your letter here.

\closing{Sincerely,}
\end{letter}

\end{document}
)")},
    };
}
}

QStringList TemplateService::templateNames() const
{
    return templates().keys();
}

QString TemplateService::contentForTemplate(const QString &templateName) const
{
    const auto allTemplates = templates();
    return allTemplates.value(templateName, allTemplates.value(QStringLiteral("Article")));
}

