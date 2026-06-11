#pragma once

#include "models/DiagnosticsModel.h"
#include "models/ProjectTreeModel.h"
#include "services/BuildManager.h"
#include "services/LatexEnvironmentService.h"
#include "services/ProjectService.h"
#include "services/SyncTexService.h"
#include "widgets/LatexEditor.h"

#include <QFileSystemModel>
#include <QHash>
#include <QMainWindow>
#include <QPointer>
#include <QTimer>

#include <memory>

#ifdef LATEXAPP_HAS_QTPDF
#include <QtPdf/QPdfDocument>
#endif

class QPdfView;
class QCheckBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QSplitter;
class QStackedWidget;
class QTableView;
class QTabWidget;
class QTemporaryDir;
class QTreeView;
class QWidget;
class QAction;

class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;
    void openStartupPath(const QString &path);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void createActions();
    void createCentralUi();
    QWidget *createLandingPage();
    QWidget *createWorkspacePage();
    QWidget *createSearchReplacePanel();
    void createStatusBar();
    void updateWindowTitle();
    void showLandingPage();
    void showWorkspacePage();
    void openProjectPath(const QString &path);
    void openStandaloneFilePath(const QString &path);
    void createStandaloneDocument(const QString &templateName);
    QString activeBuildRoot() const;
    ProjectConfig activeBuildConfig() const;
    bool preparePreviewSnapshot(QString *previewRoot, ProjectConfig *previewConfig, QString *errorMessage);
    bool writeOpenEditorsToPreview(const QString &sourceRoot, const QString &previewRoot, QString *errorMessage) const;
    QVector<Diagnostic> mapPreviewDiagnostics(QVector<Diagnostic> diagnostics) const;
    LatexEditor *currentEditor() const;
    LatexEditor *openFileInEditor(const QString &filePath, int line = 0);
    bool saveEditor(LatexEditor *editor);
    bool saveAll();
    void compactBuildPanel();
    void scheduleAutoCompile();
    void compileLivePreview();
    void loadPdf(const QString &pdfPath);
    QString currentPdfPath() const;
    QString displayedPdfPath() const;
    QString mapSourcePathToDisplayedBuildPath(const QString &sourcePath) const;
    QString mapDisplayedBuildPathToSourcePath(const QString &buildPath) const;
    void syncSourceToPdf(const QString &sourcePath, int line, int column);
    void syncPdfToSource(int oneBasedPage, const QPointF &pagePoint);
    void setProjectRootInTree(const QString &projectRoot);
    void addRecentProject(const QString &path);
    void refreshRecentProjectsMenu();
    QStringList recentProjects() const;
    QString projectTreePathFromIndex(const QModelIndex &index) const;
    QString selectedProjectTreeDirectory(const QModelIndex &index) const;
    bool isProjectItemPath(const QString &path) const;
    bool isValidProjectTreeName(const QString &name) const;
    void updateMainFileAfterPathChanged(const QString &oldPath, const QString &newPath);
    void closeEditorForPath(const QString &path);
    void rekeyOpenEditor(const QString &oldPath, const QString &newPath);
    bool findTextInCurrentEditor(bool backward);
    void updateSearchStatus(const QString &message = {});
    bool copyDirectoryRecursively(const QString &sourcePath, const QString &destinationPath, QString *errorMessage) const;
    bool copyDirectoryForPreview(const QString &sourcePath, const QString &destinationPath, const QString &sourceRoot, const QString &outputDirectory, QString *errorMessage) const;

private slots:
    void newProject();
    void newBlankDocument();
    void openFile();
    void openProject();
    void saveCurrentFile();
    void saveCurrentFileAs();
    void compileProject();
    void openCompiledPdf();
    void savePdfAs();
    void copyProject();
    void exportProjectZip();
    void showPreferences();
    void showFindPanel();
    void showReplacePanel();
    void hideSearchReplacePanel();
    void findNextMatch();
    void findPreviousMatch();
    void replaceCurrentMatch();
    void replaceAllMatches();
    void showProjectTreeContextMenu(const QPoint &position);
    void newFileInProjectTree(const QString &parentDirectory);
    void newFolderInProjectTree(const QString &parentDirectory);
    void renameProjectTreeItem(const QString &path);
    void deleteProjectTreeItem(const QString &path);
    void revealProjectTreeItem(const QString &path);
    void setMainFileFromTree(const QString &texPath);
    void refreshProjectTree();
    void onTreeActivated(const QModelIndex &index);
    void onDiagnosticActivated(const QModelIndex &index);
    void onBuildStarted();
    void onBuildOutput(const QString &text);
    void onBuildFinished(bool success, const QString &pdfPath, QVector<Diagnostic> diagnostics);

private:
    ProjectService m_projectService;
    LatexEnvironmentService m_environmentService;
    SyncTexService m_syncTexService;
    BuildManager m_buildManager;
    DiagnosticsModel m_diagnosticsModel;
    QFileSystemModel m_fileSystemModel;
    ProjectTreeModel m_projectTreeModel;
#ifdef LATEXAPP_HAS_QTPDF
    QPdfDocument m_pdfDocument;
#endif

    QStackedWidget *m_centralStack = nullptr;
    QWidget *m_landingPage = nullptr;
    QWidget *m_workspacePage = nullptr;
    QWidget *m_searchPanel = nullptr;
    QLineEdit *m_searchText = nullptr;
    QLineEdit *m_replaceText = nullptr;
    QCheckBox *m_matchCaseCheck = nullptr;
    QLabel *m_searchStatus = nullptr;
    QTreeView *m_projectTree = nullptr;
    QTabWidget *m_editorTabs = nullptr;
    QSplitter *m_editorAndBottomSplitter = nullptr;
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
    std::unique_ptr<QTemporaryDir> m_previewBuildRoot;
    std::unique_ptr<QTemporaryDir> m_pendingPreviewBuildRoot;
    QString m_previewSourceRoot;
    QString m_previewMirrorRoot;
    QString m_displayedPdfPath;
    QString m_displayedBuildRoot;
    QString m_displayedSourceRoot;
    QString m_standaloneFilePath;
    QString m_pendingPdfSavePath;
    bool m_liveCompileEnabled = true;
    bool m_currentBuildIsPreview = false;
    bool m_livePreviewQueued = false;
    bool m_displayedPdfFromPreview = false;
};
