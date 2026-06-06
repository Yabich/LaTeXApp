#pragma once

#include "models/DiagnosticsModel.h"
#include "services/BuildManager.h"
#include "services/LatexEnvironmentService.h"
#include "services/ProjectService.h"
#include "widgets/LatexEditor.h"

#include <QFileSystemModel>
#include <QHash>
#include <QMainWindow>
#include <QPointer>
#include <QTimer>

#ifdef LATEXAPP_HAS_QTPDF
#include <QtPdf/QPdfDocument>
#endif

class QPdfView;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QSplitter;
class QStackedWidget;
class QTableView;
class QTabWidget;
class QTreeView;
class QWidget;
class QAction;

class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    void openStartupPath(const QString &path);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void createActions();
    void createCentralUi();
    QWidget *createLandingPage();
    QWidget *createWorkspacePage();
    void createStatusBar();
    void updateWindowTitle();
    void showLandingPage();
    void showWorkspacePage();
    void openProjectPath(const QString &path);
    void openStandaloneFilePath(const QString &path);
    void createStandaloneDocument(const QString &templateName);
    QString activeBuildRoot() const;
    ProjectConfig activeBuildConfig() const;
    LatexEditor *currentEditor() const;
    LatexEditor *openFileInEditor(const QString &filePath, int line = 0);
    bool saveEditor(LatexEditor *editor);
    bool saveAll();
    void scheduleAutoCompile();
    void loadPdf(const QString &pdfPath);
    QString currentPdfPath() const;
    void setProjectRootInTree(const QString &projectRoot);
    void addRecentProject(const QString &path);
    void refreshRecentProjectsMenu();
    QStringList recentProjects() const;
    bool copyDirectoryRecursively(const QString &sourcePath, const QString &destinationPath, QString *errorMessage) const;

private slots:
    void newProject();
    void newBlankDocument();
    void openFile();
    void openProject();
    void saveCurrentFile();
    void saveCurrentFileAs();
    void compileProject();
    void openCompiledPdf();
    void copyProject();
    void exportProjectZip();
    void showPreferences();
    void onTreeActivated(const QModelIndex &index);
    void onDiagnosticActivated(const QModelIndex &index);
    void onBuildStarted();
    void onBuildOutput(const QString &text);
    void onBuildFinished(bool success, const QString &pdfPath, QVector<Diagnostic> diagnostics);

private:
    ProjectService m_projectService;
    LatexEnvironmentService m_environmentService;
    BuildManager m_buildManager;
    DiagnosticsModel m_diagnosticsModel;
    QFileSystemModel m_fileSystemModel;
#ifdef LATEXAPP_HAS_QTPDF
    QPdfDocument m_pdfDocument;
#endif

    QStackedWidget *m_centralStack = nullptr;
    QWidget *m_landingPage = nullptr;
    QWidget *m_workspacePage = nullptr;
    QTreeView *m_projectTree = nullptr;
    QTabWidget *m_editorTabs = nullptr;
#ifdef LATEXAPP_HAS_QTPDF
    QPdfView *m_pdfView = nullptr;
#else
    QLabel *m_pdfPlaceholder = nullptr;
#endif
    QPlainTextEdit *m_buildOutput = nullptr;
    QTableView *m_diagnosticsView = nullptr;
    QMenu *m_recentProjectsMenu = nullptr;
    QAction *m_autoCompileAction = nullptr;
    QTimer m_autoCompileTimer;
    QHash<QString, QPointer<LatexEditor>> m_openEditors;
    QString m_standaloneFilePath;
};
