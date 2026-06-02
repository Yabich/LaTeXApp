#pragma once

#include <QString>

enum class DiagnosticSeverity {
    Info,
    Warning,
    Error
};

struct Diagnostic {
    DiagnosticSeverity severity = DiagnosticSeverity::Info;
    QString filePath;
    int line = 0;
    QString message;
};

