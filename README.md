# LaTeXApp

LaTeXApp is a Windows-only desktop LaTeX editor MVP written primarily in C++ with Qt 6. It is designed as a local, solo-user editor with an Overleaf-like workflow: project tree, tabbed LaTeX editing, PDF preview, compile diagnostics, templates, and guided Windows TeX detection.

## Current Features

- Native Qt 6 Windows desktop shell.
- Folder-based projects with optional `.latexapp/project.json`.
- Template project creation for article, report, thesis, beamer, and letter.
- Tabbed editor with line numbers, LaTeX syntax highlighting, search, and save support.
- `latexmk`-based build runner using `QProcess`.
- Parsed diagnostics for common `file:line: error` and warning formats.
- PDF preview using `QtPdfWidgets`.
- Windows TeX detection for MiKTeX, TeX Live, TinyTeX, and tools on `PATH`.
- Settings dialog for overriding `latexmk`.
- Inno Setup installer template.
- Unit tests for config, templates, diagnostics parsing, and environment detection.

## Build Requirements

- Windows 10 or newer.
- Visual Studio 2022 Build Tools with MSVC
- CMake 3.26 or newer.
- Qt 6.6 or newer with `Widgets`, `Pdf`, and `PdfWidgets`.
- Optional for compiling documents: MiKTeX, TeX Live, or TinyTeX with `latexmk`.
- Optional for installer: Inno Setup 6.

## Configure And Build

Recommended MSVC build:

```powershell
.\scripts\configure.ps1 -QtPrefix "C:\Qt\6.11.1\msvc2022_64"
.\scripts\build.ps1
.\scripts\deploy.ps1
.\scripts\test.ps1
```

The helper supports newer Visual Studio installations by entering the Visual Studio developer environment and using CMake's `NMake Makefiles` generator. This avoids requiring your CMake version to know about a specific Visual Studio release generator.

Manual equivalent from a Visual Studio Developer PowerShell:

```powershell
cmake -S . -B build -G "NMake Makefiles" -DCMAKE_PREFIX_PATH="C:\Qt\6.11.1\msvc2022_64"
cmake --build build
```

## Run

Run `scripts\deploy.ps1` after building before launching from Explorer. It copies Qt DLLs and plugins, including the required Windows platform plugin, beside `LaTeXApp.exe`.

```powershell
.\build\LaTeXApp.exe
.\build\LaTeXApp.exe C:\path\to\project-or-main.tex
```

Or use the helper:

```powershell
.\scripts\run.ps1
.\scripts\run.ps1 -Path C:\path\to\project-or-main.tex
```
