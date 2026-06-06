#include "ui/MainWindow.h"

#include "services/TemplateService.h"
#include "ui/SettingsDialog.h"

#include <QAction>
#include <QApplication>
#include <QAbstractItemView>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QSettings>
#include <QSizePolicy>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTabWidget>
#include <QTableView>
#include <QTemporaryDir>
#include <QTextDocument>
#include <QToolBar>
#include <QTreeView>
#include <QUrl>
#include <QVBoxLayout>

#ifdef LATEXAPP_HAS_QTPDF
#include <QtPdf/QPdfPageNavigator>
#include <QtPdfWidgets/QPdfView>
#endif

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    QApplication::setOrganizationName(QStringLiteral("LaTeXApp"));
    QApplication::setApplicationName(QStringLiteral("LaTeXApp"));

    m_autoCompileTimer.setInterval(1200);
    m_autoCompileTimer.setSingleShot(true);
    connect(&m_autoCompileTimer, &QTimer::timeout, this, &MainWindow::compileLivePreview);

    connect(&m_buildManager, &BuildManager::buildStarted, this, &MainWindow::onBuildStarted);
    connect(&m_buildManager, &BuildManager::buildOutput, this, &MainWindow::onBuildOutput);
    connect(&m_buildManager, &BuildManager::buildFinished, this, &MainWindow::onBuildFinished);

    connect(&m_projectService, &ProjectService::projectChanged, this, [this](const QString &projectRoot) {
        setProjectRootInTree(projectRoot);
        if (m_autoCompileAction) {
            QSignalBlocker blocker(m_autoCompileAction);
            m_autoCompileAction->setChecked(m_liveCompileEnabled);
        }
        updateWindowTitle();
        addRecentProject(projectRoot);
        openFileInEditor(m_projectService.mainFilePath());
        showWorkspacePage();
    });

    createActions();
    createCentralUi();
    createStatusBar();
    refreshRecentProjectsMenu();
    updateWindowTitle();
    resize(1500, 920);
}

MainWindow::~MainWindow() = default;

void MainWindow::openStartupPath(const QString &path)
{
    if (!path.isEmpty()) {
        QFileInfo info(path);
        if (info.isFile()) {
            openStandaloneFilePath(path);
        } else {
            openProjectPath(path);
        }
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (saveAll()) {
        event->accept();
    } else {
        event->ignore();
    }
}

void MainWindow::createActions()
{
    auto *fileMenu = menuBar()->addMenu(QStringLiteral("&File"));
    auto *projectMenu = menuBar()->addMenu(QStringLiteral("&Project"));
    auto *toolsMenu = menuBar()->addMenu(QStringLiteral("&Tools"));

    auto *toolbar = addToolBar(QStringLiteral("Main"));
    toolbar->setMovable(false);

    const auto addAction = [this, toolbar](QMenu *menu, const QString &text, const QKeySequence &shortcut, auto slot) {
        auto *action = new QAction(text, this);
        action->setShortcut(shortcut);
        connect(action, &QAction::triggered, this, slot);
        menu->addAction(action);
        toolbar->addAction(action);
        return action;
    };

    addAction(fileMenu, QStringLiteral("New Project"), QKeySequence::New, &MainWindow::newProject);
    addAction(fileMenu, QStringLiteral("Open File"), QKeySequence::Open, &MainWindow::openFile);
    addAction(fileMenu, QStringLiteral("Open Project"), QKeySequence(QStringLiteral("Ctrl+Shift+O")), &MainWindow::openProject);
    m_recentProjectsMenu = fileMenu->addMenu(QStringLiteral("Recent Projects"));
    fileMenu->addSeparator();
    addAction(fileMenu, QStringLiteral("Save"), QKeySequence::Save, &MainWindow::saveCurrentFile);
    addAction(fileMenu, QStringLiteral("Save As"), QKeySequence::SaveAs, &MainWindow::saveCurrentFileAs);
    fileMenu->addSeparator();
    auto *exitAction = fileMenu->addAction(QStringLiteral("Exit"));
    connect(exitAction, &QAction::triggered, this, &QWidget::close);

    addAction(projectMenu, QStringLiteral("Compile"), QKeySequence(QStringLiteral("Ctrl+R")), &MainWindow::compileProject);
    addAction(projectMenu, QStringLiteral("Open Compiled PDF"), QKeySequence(), &MainWindow::openCompiledPdf);
    projectMenu->addSeparator();
    addAction(projectMenu, QStringLiteral("Copy Project"), QKeySequence(), &MainWindow::copyProject);
    addAction(projectMenu, QStringLiteral("Export Project ZIP"), QKeySequence(), &MainWindow::exportProjectZip);
    projectMenu->addSeparator();
    m_autoCompileAction = projectMenu->addAction(QStringLiteral("Live Compile on Edit"));
    m_autoCompileAction->setCheckable(true);
    m_autoCompileAction->setChecked(true);
    connect(m_autoCompileAction, &QAction::toggled, this, [this](bool enabled) {
        m_liveCompileEnabled = enabled;
        if (!enabled) {
            m_autoCompileTimer.stop();
            m_livePreviewQueued = false;
        }
    });
    addAction(toolsMenu, QStringLiteral("Preferences"), QKeySequence::Preferences, &MainWindow::showPreferences);
}

void MainWindow::createCentralUi()
{
    m_centralStack = new QStackedWidget(this);
    m_landingPage = createLandingPage();
    m_workspacePage = createWorkspacePage();

    m_centralStack->addWidget(m_landingPage);
    m_centralStack->addWidget(m_workspacePage);
    setCentralWidget(m_centralStack);
    showLandingPage();
}

QWidget *MainWindow::createLandingPage()
{
    auto *page = new QWidget(this);
    page->setObjectName(QStringLiteral("LandingPage"));
    page->setStyleSheet(QStringLiteral(
        "QWidget#LandingPage { background: #f6f7f9; }"
        "QLabel#LandingTitle { color: #111827; font-size: 34px; font-weight: 700; }"
        "QLabel#LandingSubtitle { color: #4b5563; font-size: 14px; }"
        "QFrame#LandingPanel { background: #ffffff; border: 1px solid #d8dee8; border-radius: 8px; }"
        "QLabel#PanelTitle { color: #111827; font-size: 17px; font-weight: 600; }"
        "QLabel#PanelText { color: #4b5563; font-size: 12px; }"
        "QPushButton { background: #ffffff; color: #111827; border: 1px solid #cfd6e2; border-radius: 6px; padding: 10px 14px; text-align: left; }"
        "QPushButton:hover { background: #f1f5f9; border-color: #9aa8bb; }"
        "QPushButton:pressed { background: #e8eef6; }"
        "QPushButton#PrimaryButton { background: #1f2937; color: #ffffff; border-color: #1f2937; font-weight: 600; }"
        "QPushButton#PrimaryButton:hover { background: #111827; }"
    ));

    auto *outerLayout = new QVBoxLayout(page);
    outerLayout->setContentsMargins(72, 56, 72, 56);
    outerLayout->setSpacing(28);

    auto *title = new QLabel(QStringLiteral("LaTeXApp"), page);
    title->setObjectName(QStringLiteral("LandingTitle"));
    auto *subtitle = new QLabel(QStringLiteral("Create a new LaTeX document, open a single file, or continue from a project folder."), page);
    subtitle->setObjectName(QStringLiteral("LandingSubtitle"));
    subtitle->setWordWrap(true);

    outerLayout->addWidget(title);
    outerLayout->addWidget(subtitle);

    auto *contentLayout = new QHBoxLayout();
    contentLayout->setSpacing(24);
    outerLayout->addLayout(contentLayout, 1);

    auto *quickPanel = new QFrame(page);
    quickPanel->setObjectName(QStringLiteral("LandingPanel"));
    quickPanel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    auto *quickLayout = new QVBoxLayout(quickPanel);
    quickLayout->setContentsMargins(24, 22, 24, 24);
    quickLayout->setSpacing(12);

    auto *quickTitle = new QLabel(QStringLiteral("Start"), quickPanel);
    quickTitle->setObjectName(QStringLiteral("PanelTitle"));
    auto *quickText = new QLabel(QStringLiteral("Open existing work or create a fresh file."), quickPanel);
    quickText->setObjectName(QStringLiteral("PanelText"));
    quickText->setWordWrap(true);
    quickLayout->addWidget(quickTitle);
    quickLayout->addWidget(quickText);

    auto *openFileButton = new QPushButton(QStringLiteral("Open File..."), quickPanel);
    openFileButton->setObjectName(QStringLiteral("PrimaryButton"));
    connect(openFileButton, &QPushButton::clicked, this, &MainWindow::openFile);
    quickLayout->addWidget(openFileButton);

    auto *openProjectButton = new QPushButton(QStringLiteral("Open Folder..."), quickPanel);
    connect(openProjectButton, &QPushButton::clicked, this, &MainWindow::openProject);
    quickLayout->addWidget(openProjectButton);

    auto *blankButton = new QPushButton(QStringLiteral("Create Blank Document..."), quickPanel);
    connect(blankButton, &QPushButton::clicked, this, &MainWindow::newBlankDocument);
    quickLayout->addWidget(blankButton);

    auto *projectButton = new QPushButton(QStringLiteral("Create Project From Template..."), quickPanel);
    connect(projectButton, &QPushButton::clicked, this, &MainWindow::newProject);
    quickLayout->addWidget(projectButton);
    quickLayout->addStretch(1);
    contentLayout->addWidget(quickPanel, 1);

    auto *templatePanel = new QFrame(page);
    templatePanel->setObjectName(QStringLiteral("LandingPanel"));
    templatePanel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto *templateLayout = new QVBoxLayout(templatePanel);
    templateLayout->setContentsMargins(24, 22, 24, 24);
    templateLayout->setSpacing(12);

    auto *templateTitle = new QLabel(QStringLiteral("New Document From Template"), templatePanel);
    templateTitle->setObjectName(QStringLiteral("PanelTitle"));
    auto *templateText = new QLabel(QStringLiteral("Choose a starter file, save it anywhere, and begin editing immediately."), templatePanel);
    templateText->setObjectName(QStringLiteral("PanelText"));
    templateText->setWordWrap(true);
    templateLayout->addWidget(templateTitle);
    templateLayout->addWidget(templateText);

    auto *templateGrid = new QGridLayout();
    templateGrid->setHorizontalSpacing(12);
    templateGrid->setVerticalSpacing(12);
    TemplateService templates;
    const auto names = templates.templateNames();
    for (int i = 0; i < names.size(); ++i) {
        const auto name = names.at(i);
        auto *button = new QPushButton(name, templatePanel);
        button->setMinimumHeight(52);
        connect(button, &QPushButton::clicked, this, [this, name]() { createStandaloneDocument(name); });
        templateGrid->addWidget(button, i / 2, i % 2);
    }
    templateLayout->addLayout(templateGrid);
    templateLayout->addStretch(1);
    contentLayout->addWidget(templatePanel, 2);

    outerLayout->addStretch(1);
    return page;
}

QWidget *MainWindow::createWorkspacePage()
{
    m_projectTree = new QTreeView(this);
    m_projectTree->setHeaderHidden(true);
    m_fileSystemModel.setFilter(QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot);
    m_fileSystemModel.setNameFilters({QStringLiteral("*.tex"), QStringLiteral("*.bib"), QStringLiteral("*.sty"), QStringLiteral("*.cls"), QStringLiteral("*.png"), QStringLiteral("*.jpg"), QStringLiteral("*.pdf")});
    m_fileSystemModel.setNameFilterDisables(false);
    m_projectTree->setModel(&m_fileSystemModel);
    connect(m_projectTree, &QTreeView::doubleClicked, this, &MainWindow::onTreeActivated);

    m_editorTabs = new QTabWidget(this);
    m_editorTabs->setTabsClosable(true);
    m_editorTabs->setDocumentMode(true);
    connect(m_editorTabs, &QTabWidget::tabCloseRequested, this, [this](int index) {
        auto *editor = qobject_cast<LatexEditor *>(m_editorTabs->widget(index));
        if (!editor || !saveEditor(editor)) {
            return;
        }
        m_openEditors.remove(editor->filePath());
        m_editorTabs->removeTab(index);
        editor->deleteLater();
    });

#ifdef LATEXAPP_HAS_QTPDF
    auto *pdfPane = new QPdfView(this);
    m_pdfView = pdfPane;
    m_pdfView->setDocument(&m_pdfDocument);
    m_pdfView->setPageMode(QPdfView::PageMode::MultiPage);
    m_pdfView->setZoomMode(QPdfView::ZoomMode::FitToWidth);
#else
    auto *pdfPane = new QLabel(QStringLiteral("Embedded PDF preview is not available because the Qt PDF module is not installed.\n\nCompile the project, then use Project > Open Compiled PDF."), this);
    m_pdfPlaceholder = pdfPane;
    m_pdfPlaceholder->setAlignment(Qt::AlignCenter);
    m_pdfPlaceholder->setWordWrap(true);
    m_pdfPlaceholder->setStyleSheet(QStringLiteral("QLabel { color: #4b5563; background: #f8fafc; border-left: 1px solid #d8dee8; padding: 24px; }"));
#endif

    m_buildOutput = new QPlainTextEdit(this);
    m_buildOutput->setReadOnly(true);
    m_buildOutput->setMaximumBlockCount(4000);

    m_diagnosticsView = new QTableView(this);
    m_diagnosticsView->setModel(&m_diagnosticsModel);
    m_diagnosticsView->horizontalHeader()->setStretchLastSection(true);
    m_diagnosticsView->verticalHeader()->hide();
    m_diagnosticsView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_diagnosticsView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    connect(m_diagnosticsView, &QTableView::doubleClicked, this, &MainWindow::onDiagnosticActivated);

    auto *bottomTabs = new QTabWidget(this);
    bottomTabs->addTab(m_diagnosticsView, QStringLiteral("Diagnostics"));
    bottomTabs->addTab(m_buildOutput, QStringLiteral("Build Log"));

    auto *editorAndBottom = new QSplitter(Qt::Vertical, this);
    editorAndBottom->addWidget(m_editorTabs);
    editorAndBottom->addWidget(bottomTabs);
    editorAndBottom->setStretchFactor(0, 4);
    editorAndBottom->setStretchFactor(1, 1);

    auto *mainSplitter = new QSplitter(Qt::Horizontal, this);
    mainSplitter->addWidget(m_projectTree);
    mainSplitter->addWidget(editorAndBottom);
    mainSplitter->addWidget(pdfPane);
    mainSplitter->setStretchFactor(0, 1);
    mainSplitter->setStretchFactor(1, 4);
    mainSplitter->setStretchFactor(2, 3);

    return mainSplitter;
}

void MainWindow::createStatusBar()
{
    statusBar()->addPermanentWidget(new QLabel(QStringLiteral("Windows MVP"), this));
    statusBar()->showMessage(QStringLiteral("Ready"));
}

void MainWindow::updateWindowTitle()
{
    if (!m_standaloneFilePath.isEmpty()) {
        setWindowTitle(QStringLiteral("%1 - LaTeXApp").arg(QFileInfo(m_standaloneFilePath).fileName()));
        return;
    }

    const auto root = m_projectService.projectRoot();
    setWindowTitle(root.isEmpty()
        ? QStringLiteral("LaTeXApp")
        : QStringLiteral("%1 - LaTeXApp").arg(QFileInfo(root).fileName()));
}

void MainWindow::showLandingPage()
{
    menuBar()->hide();
    const auto toolbars = findChildren<QToolBar *>();
    for (auto *toolbar : toolbars) {
        toolbar->hide();
    }

    if (m_centralStack && m_landingPage) {
        m_centralStack->setCurrentWidget(m_landingPage);
    }
}

void MainWindow::showWorkspacePage()
{
    menuBar()->show();
    const auto toolbars = findChildren<QToolBar *>();
    for (auto *toolbar : toolbars) {
        toolbar->show();
    }

    if (m_centralStack && m_workspacePage) {
        m_centralStack->setCurrentWidget(m_workspacePage);
    }
}

void MainWindow::openProjectPath(const QString &path)
{
    if (!saveAll()) {
        return;
    }

    m_standaloneFilePath.clear();
    m_projectTree->setVisible(true);

    QString error;
    if (!m_projectService.openPath(path, &error)) {
        QMessageBox::warning(this, QStringLiteral("Open Failed"), error);
    }
}

void MainWindow::openStandaloneFilePath(const QString &path)
{
    if (!saveAll()) {
        return;
    }

    QFileInfo info(path);
    if (!info.exists() || !info.isFile()) {
        QMessageBox::warning(this, QStringLiteral("Open Failed"), QStringLiteral("The selected file does not exist."));
        return;
    }

    auto *editor = openFileInEditor(info.absoluteFilePath());
    if (!editor) {
        return;
    }

    m_standaloneFilePath = QFileInfo(editor->filePath()).absoluteFilePath();
    m_projectTree->setVisible(false);
    if (m_autoCompileAction) {
        QSignalBlocker blocker(m_autoCompileAction);
        m_autoCompileAction->setChecked(m_liveCompileEnabled);
    }
    updateWindowTitle();
    showWorkspacePage();
    statusBar()->showMessage(QStringLiteral("Opened standalone file"), 3000);
}

void MainWindow::createStandaloneDocument(const QString &templateName)
{
    const auto suggestedName = templateName == QStringLiteral("Blank")
        ? QStringLiteral("untitled.tex")
        : QStringLiteral("%1.tex").arg(templateName.toLower());
    const auto path = QFileDialog::getSaveFileName(this,
        QStringLiteral("Create LaTeX Document"),
        QDir::home().filePath(suggestedName),
        QStringLiteral("TeX files (*.tex);;All files (*.*)"));
    if (path.isEmpty()) {
        return;
    }

    if (!saveAll()) {
        return;
    }

    TemplateService templates;
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, QStringLiteral("Create Document Failed"), file.errorString());
        return;
    }
    file.write(templates.contentForTemplate(templateName).toUtf8());
    file.close();

    openStandaloneFilePath(path);
}

QString MainWindow::activeBuildRoot() const
{
    if (!m_standaloneFilePath.isEmpty()) {
        return QFileInfo(m_standaloneFilePath).absolutePath();
    }
    return m_projectService.projectRoot();
}

ProjectConfig MainWindow::activeBuildConfig() const
{
    if (!m_standaloneFilePath.isEmpty()) {
        auto config = ProjectConfig::defaults();
        const auto fileInfo = QFileInfo(m_standaloneFilePath);
        config.mainFile = fileInfo.fileName();
        return config;
    }
    return m_projectService.config();
}

bool MainWindow::preparePreviewSnapshot(QString *previewRoot, ProjectConfig *previewConfig, QString *errorMessage)
{
    const auto sourceRoot = activeBuildRoot();
    if (sourceRoot.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("No file or project is open.");
        }
        return false;
    }

    const auto config = activeBuildConfig();

    m_pendingPreviewBuildRoot = std::make_unique<QTemporaryDir>();
    if (!m_pendingPreviewBuildRoot->isValid()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Could not create a temporary preview folder.");
        }
        return false;
    }

    const auto mirrorRoot = m_pendingPreviewBuildRoot->path();
    const auto sourceCanonical = QFileInfo(sourceRoot).canonicalFilePath();
    if (!m_standaloneFilePath.isEmpty()) {
        const auto mainSourcePath = QFileInfo(m_standaloneFilePath).canonicalFilePath();
        const auto mainTargetPath = QDir(mirrorRoot).filePath(QFileInfo(mainSourcePath).fileName());
        if (!QFile::copy(mainSourcePath, mainTargetPath)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("Could not copy the source file into the preview folder.");
            }
            return false;
        }
    } else {
        if (!copyDirectoryForPreview(sourceCanonical, mirrorRoot, sourceCanonical, config.outputDirectory, errorMessage)) {
            return false;
        }
    }

    if (!writeOpenEditorsToPreview(sourceCanonical, mirrorRoot, errorMessage)) {
        return false;
    }

    m_previewSourceRoot = sourceCanonical;
    m_previewMirrorRoot = mirrorRoot;
    if (previewRoot) {
        *previewRoot = mirrorRoot;
    }
    if (previewConfig) {
        *previewConfig = config;
    }
    return true;
}

bool MainWindow::writeOpenEditorsToPreview(const QString &sourceRoot, const QString &previewRoot, QString *errorMessage) const
{
    const QDir sourceDirectory(sourceRoot);
    const QDir previewDirectory(previewRoot);

    for (auto it = m_openEditors.cbegin(); it != m_openEditors.cend(); ++it) {
        auto *editor = it.value().data();
        if (!editor) {
            continue;
        }

        const auto editorPath = QFileInfo(editor->filePath()).canonicalFilePath();
        const auto relativePath = sourceDirectory.relativeFilePath(editorPath);
        if (relativePath.startsWith(QStringLiteral("..")) || QFileInfo(relativePath).isAbsolute()) {
            continue;
        }

        const auto targetPath = previewDirectory.filePath(relativePath);
        QDir targetDirectory = QFileInfo(targetPath).absoluteDir();
        if (!targetDirectory.exists() && !targetDirectory.mkpath(QStringLiteral("."))) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("Could not create preview folder for %1.").arg(relativePath);
            }
            return false;
        }

        QFile targetFile(targetPath);
        if (!targetFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            if (errorMessage) {
                *errorMessage = targetFile.errorString();
            }
            return false;
        }
        targetFile.write(editor->toPlainText().toUtf8());
    }

    return true;
}

QVector<Diagnostic> MainWindow::mapPreviewDiagnostics(QVector<Diagnostic> diagnostics) const
{
    if (!m_currentBuildIsPreview || m_previewMirrorRoot.isEmpty() || m_previewSourceRoot.isEmpty()) {
        return diagnostics;
    }

    const QDir previewDirectory(m_previewMirrorRoot);
    const QDir sourceDirectory(m_previewSourceRoot);
    for (auto &diagnostic : diagnostics) {
        if (diagnostic.filePath.isEmpty()) {
            continue;
        }

        const auto relativePath = previewDirectory.relativeFilePath(diagnostic.filePath);
        if (relativePath.startsWith(QStringLiteral("..")) || QFileInfo(relativePath).isAbsolute()) {
            continue;
        }
        diagnostic.filePath = sourceDirectory.filePath(relativePath);
    }

    return diagnostics;
}

LatexEditor *MainWindow::currentEditor() const
{
    return qobject_cast<LatexEditor *>(m_editorTabs->currentWidget());
}

LatexEditor *MainWindow::openFileInEditor(const QString &filePath, int line)
{
    if (filePath.isEmpty()) {
        return nullptr;
    }

    const QFileInfo info(filePath);
    if (!info.exists() || info.isDir()) {
        return nullptr;
    }

    const auto canonicalPath = info.canonicalFilePath();
    if (m_openEditors.contains(canonicalPath) && m_openEditors.value(canonicalPath)) {
        auto *editor = m_openEditors.value(canonicalPath).data();
        m_editorTabs->setCurrentWidget(editor);
        editor->gotoLine(line);
        return editor;
    }

    auto *editor = new LatexEditor(this);
    QString error;
    if (!editor->loadFromFile(canonicalPath, &error)) {
        editor->deleteLater();
        QMessageBox::warning(this, QStringLiteral("Open File Failed"), error);
        return nullptr;
    }

    connect(editor->document(), &QTextDocument::modificationChanged, this, [this, editor](bool modified) {
        const auto index = m_editorTabs->indexOf(editor);
        if (index >= 0) {
            const auto name = QFileInfo(editor->filePath()).fileName();
            m_editorTabs->setTabText(index, modified ? QStringLiteral("*%1").arg(name) : name);
        }
    });
    connect(editor->document(), &QTextDocument::contentsChanged, this, &MainWindow::scheduleAutoCompile);

    m_openEditors.insert(canonicalPath, editor);
    m_editorTabs->addTab(editor, info.fileName());
    m_editorTabs->setCurrentWidget(editor);
    editor->gotoLine(line);
    return editor;
}

bool MainWindow::saveEditor(LatexEditor *editor)
{
    if (!editor || !editor->document()->isModified()) {
        return true;
    }

    const auto choice = QMessageBox::question(this,
        QStringLiteral("Save Changes"),
        QStringLiteral("Save changes to %1?").arg(QFileInfo(editor->filePath()).fileName()),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
        QMessageBox::Save);

    if (choice == QMessageBox::Cancel) {
        return false;
    }
    if (choice == QMessageBox::Discard) {
        editor->document()->setModified(false);
        return true;
    }

    QString error;
    if (!editor->save(&error)) {
        QMessageBox::warning(this, QStringLiteral("Save Failed"), error);
        return false;
    }
    return true;
}

bool MainWindow::saveAll()
{
    for (int i = 0; i < m_editorTabs->count(); ++i) {
        if (!saveEditor(qobject_cast<LatexEditor *>(m_editorTabs->widget(i)))) {
            return false;
        }
    }
    return true;
}

void MainWindow::scheduleAutoCompile()
{
    if (!m_liveCompileEnabled || activeBuildRoot().isEmpty()) {
        return;
    }
    m_autoCompileTimer.start();
}

void MainWindow::compileLivePreview()
{
    if (!m_liveCompileEnabled || activeBuildRoot().isEmpty()) {
        return;
    }

    if (m_buildManager.isRunning()) {
        m_livePreviewQueued = true;
        statusBar()->showMessage(QStringLiteral("Live preview queued..."), 3000);
        return;
    }

    m_livePreviewQueued = false;

    QString previewRoot;
    ProjectConfig previewConfig;
    QString error;
    if (!preparePreviewSnapshot(&previewRoot, &previewConfig, &error)) {
        statusBar()->showMessage(QStringLiteral("Live preview skipped: %1").arg(error), 7000);
        return;
    }

    const auto latexmk = m_environmentService.preferredLatexmkPath();
    const auto pdflatex = m_environmentService.preferredPdflatexPath();
    const auto hasPerl = m_environmentService.hasPerlAvailable();

    m_currentBuildIsPreview = true;
    if (!latexmk.isEmpty() && hasPerl) {
        m_buildManager.build(previewRoot, previewConfig, latexmk, BuildEngine::Latexmk);
        m_buildOutput->appendPlainText(QStringLiteral("Live preview is compiling an unsaved temporary snapshot.\n\n"));
        return;
    }

    if (!pdflatex.isEmpty()) {
        statusBar()->showMessage(QStringLiteral("Live preview using pdflatex fallback because Perl was not found"), 7000);
        m_buildManager.build(previewRoot, previewConfig, pdflatex, BuildEngine::PdfLatex);
        m_buildOutput->appendPlainText(QStringLiteral("Live preview is compiling an unsaved temporary snapshot.\nlatexmk requires Perl, which was not found. Falling back to pdflatex for this build.\nFor full latexmk support, install Strawberry Perl: %1\n\n")
            .arg(LatexEnvironmentService::perlDownloadUrl()));
        return;
    }

    m_currentBuildIsPreview = false;
    statusBar()->showMessage(QStringLiteral("Live preview skipped: no usable LaTeX compiler was found"), 7000);
}

void MainWindow::loadPdf(const QString &pdfPath)
{
    if (!QFileInfo::exists(pdfPath)) {
        return;
    }
#ifdef LATEXAPP_HAS_QTPDF
    int currentPage = 0;
    QPointF currentLocation;
    if (m_pdfView && m_pdfView->pageNavigator()) {
        currentPage = m_pdfView->pageNavigator()->currentPage();
        currentLocation = m_pdfView->pageNavigator()->currentLocation();
    }

    m_pdfDocument.close();
    const auto result = m_pdfDocument.load(pdfPath);
    if (result == QPdfDocument::Error::None) {
        if (m_pdfView && m_pdfView->pageNavigator()) {
            const auto targetPage = qBound(0, currentPage, qMax(0, m_pdfDocument.pageCount() - 1));
            m_pdfView->pageNavigator()->jump(targetPage, currentLocation);
        }
        if (m_currentBuildIsPreview && m_pendingPreviewBuildRoot) {
            m_previewBuildRoot = std::move(m_pendingPreviewBuildRoot);
        }
        statusBar()->showMessage(QStringLiteral("Loaded PDF: %1").arg(QDir::toNativeSeparators(pdfPath)), 5000);
    }
#else
    if (m_pdfPlaceholder) {
        m_pdfPlaceholder->setText(QStringLiteral("PDF compiled successfully:\n%1\n\nUse Project > Open Compiled PDF to view it.")
            .arg(QDir::toNativeSeparators(pdfPath)));
    }
    if (m_currentBuildIsPreview && m_pendingPreviewBuildRoot) {
        m_previewBuildRoot = std::move(m_pendingPreviewBuildRoot);
    }
    statusBar()->showMessage(QStringLiteral("PDF ready: %1").arg(QDir::toNativeSeparators(pdfPath)), 5000);
#endif
}

QString MainWindow::currentPdfPath() const
{
    const auto root = activeBuildRoot();
    if (root.isEmpty()) {
        return {};
    }
    const auto config = activeBuildConfig();
    const auto mainBaseName = QFileInfo(config.mainFile).completeBaseName();
    return QDir(root).filePath(QDir(config.outputDirectory).filePath(mainBaseName + QStringLiteral(".pdf")));
}

void MainWindow::setProjectRootInTree(const QString &projectRoot)
{
    const auto rootIndex = m_fileSystemModel.setRootPath(projectRoot);
    m_projectTree->setRootIndex(rootIndex);
    for (int column = 1; column < m_fileSystemModel.columnCount(); ++column) {
        m_projectTree->hideColumn(column);
    }
}

void MainWindow::addRecentProject(const QString &path)
{
    if (path.isEmpty()) {
        return;
    }
    auto recent = recentProjects();
    recent.removeAll(path);
    recent.prepend(path);
    while (recent.size() > 8) {
        recent.removeLast();
    }
    QSettings settings;
    settings.setValue(QStringLiteral("recentProjects"), recent);
    refreshRecentProjectsMenu();
}

void MainWindow::refreshRecentProjectsMenu()
{
    if (!m_recentProjectsMenu) {
        return;
    }
    m_recentProjectsMenu->clear();
    const auto recent = recentProjects();
    if (recent.isEmpty()) {
        auto *empty = m_recentProjectsMenu->addAction(QStringLiteral("No recent projects"));
        empty->setEnabled(false);
        return;
    }

    for (const auto &path : recent) {
        auto *action = m_recentProjectsMenu->addAction(QDir::toNativeSeparators(path));
        connect(action, &QAction::triggered, this, [this, path]() { openProjectPath(path); });
    }
}

QStringList MainWindow::recentProjects() const
{
    QSettings settings;
    return settings.value(QStringLiteral("recentProjects")).toStringList();
}

bool MainWindow::copyDirectoryRecursively(const QString &sourcePath, const QString &destinationPath, QString *errorMessage) const
{
    QDir source(sourcePath);
    if (!source.exists()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Source project folder does not exist.");
        }
        return false;
    }

    QDir destination(destinationPath);
    if (!destination.exists() && !destination.mkpath(QStringLiteral("."))) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Could not create destination folder.");
        }
        return false;
    }

    const auto entries = source.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries);
    for (const auto &entry : entries) {
        const auto targetPath = destination.filePath(entry.fileName());
        if (entry.isDir()) {
            if (!copyDirectoryRecursively(entry.absoluteFilePath(), targetPath, errorMessage)) {
                return false;
            }
        } else {
            QFile::remove(targetPath);
            if (!QFile::copy(entry.absoluteFilePath(), targetPath)) {
                if (errorMessage) {
                    *errorMessage = QStringLiteral("Could not copy %1.").arg(entry.fileName());
                }
                return false;
            }
        }
    }

    return true;
}

bool MainWindow::copyDirectoryForPreview(const QString &sourcePath, const QString &destinationPath, const QString &sourceRoot, const QString &outputDirectory, QString *errorMessage) const
{
    QDir source(sourcePath);
    if (!source.exists()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Source folder does not exist.");
        }
        return false;
    }

    QDir destination(destinationPath);
    if (!destination.exists() && !destination.mkpath(QStringLiteral("."))) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Could not create preview folder.");
        }
        return false;
    }

    const QDir rootDirectory(sourceRoot);
    const auto normalizedOutputDirectory = QDir::cleanPath(outputDirectory);
    const auto entries = source.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries);
    for (const auto &entry : entries) {
        const auto relativePath = QDir::cleanPath(rootDirectory.relativeFilePath(entry.absoluteFilePath()));
        if (!normalizedOutputDirectory.isEmpty()
            && normalizedOutputDirectory != QStringLiteral(".")
            && (relativePath == normalizedOutputDirectory || relativePath.startsWith(normalizedOutputDirectory + QLatin1Char('/')))) {
            continue;
        }

        const auto targetPath = destination.filePath(entry.fileName());
        if (entry.isDir()) {
            if (!copyDirectoryForPreview(entry.absoluteFilePath(), targetPath, sourceRoot, outputDirectory, errorMessage)) {
                return false;
            }
        } else {
            QFile::remove(targetPath);
            if (!QFile::copy(entry.absoluteFilePath(), targetPath)) {
                if (errorMessage) {
                    *errorMessage = QStringLiteral("Could not copy %1 into the preview folder.").arg(entry.fileName());
                }
                return false;
            }
        }
    }

    return true;
}

void MainWindow::newProject()
{
    TemplateService templates;
    bool ok = false;
    const auto templateName = QInputDialog::getItem(this,
        QStringLiteral("New Project"),
        QStringLiteral("Template"),
        templates.templateNames(),
        0,
        false,
        &ok);
    if (!ok) {
        return;
    }

    const auto directory = QFileDialog::getExistingDirectory(this, QStringLiteral("Select or Create Project Folder"));
    if (directory.isEmpty()) {
        return;
    }

    if (!saveAll()) {
        return;
    }

    m_standaloneFilePath.clear();
    m_projectTree->setVisible(true);

    QString error;
    if (!m_projectService.createProject(directory, templateName, &error)) {
        QMessageBox::warning(this, QStringLiteral("Project Creation Failed"), error);
        return;
    }
}

void MainWindow::newBlankDocument()
{
    createStandaloneDocument(QStringLiteral("Blank"));
}

void MainWindow::openFile()
{
    const auto path = QFileDialog::getOpenFileName(this,
        QStringLiteral("Open LaTeX File"),
        {},
        QStringLiteral("LaTeX files (*.tex);;BibTeX files (*.bib);;All files (*.*)"));
    if (!path.isEmpty()) {
        openStandaloneFilePath(path);
    }
}

void MainWindow::openProject()
{
    const auto path = QFileDialog::getExistingDirectory(this, QStringLiteral("Open Project Folder"));
    if (!path.isEmpty()) {
        openProjectPath(path);
    }
}

void MainWindow::saveCurrentFile()
{
    auto *editor = currentEditor();
    if (!editor) {
        return;
    }

    QString error;
    if (!editor->save(&error)) {
        QMessageBox::warning(this, QStringLiteral("Save Failed"), error);
    }
}

void MainWindow::saveCurrentFileAs()
{
    auto *editor = currentEditor();
    if (!editor) {
        return;
    }
    const auto path = QFileDialog::getSaveFileName(this, QStringLiteral("Save LaTeX File"), editor->filePath(), QStringLiteral("TeX files (*.tex);;All files (*.*)"));
    if (path.isEmpty()) {
        return;
    }

    QString error;
    if (!editor->saveAs(path, &error)) {
        QMessageBox::warning(this, QStringLiteral("Save Failed"), error);
    }
}

void MainWindow::compileProject()
{
    m_autoCompileTimer.stop();
    m_livePreviewQueued = false;
    m_currentBuildIsPreview = false;
    const auto buildRoot = activeBuildRoot();
    const auto buildConfig = activeBuildConfig();

    if (buildRoot.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("No File"), QStringLiteral("Open a LaTeX file or project before compiling."));
        return;
    }
    if (!saveAll()) {
        return;
    }

    const auto latexmk = m_environmentService.preferredLatexmkPath();
    const auto pdflatex = m_environmentService.preferredPdflatexPath();
    const auto hasPerl = m_environmentService.hasPerlAvailable();

    if (!latexmk.isEmpty() && hasPerl) {
        m_buildManager.build(buildRoot, buildConfig, latexmk, BuildEngine::Latexmk);
        return;
    }

    if (!pdflatex.isEmpty()) {
        statusBar()->showMessage(QStringLiteral("Using pdflatex fallback because Perl was not found"), 7000);
        m_buildManager.build(buildRoot, buildConfig, pdflatex, BuildEngine::PdfLatex);
        m_buildOutput->appendPlainText(QStringLiteral("latexmk requires Perl, which was not found. Falling back to pdflatex for this build.\nFor full latexmk support, install Strawberry Perl: %1\n\n")
            .arg(LatexEnvironmentService::perlDownloadUrl()));
        return;
    }

    if (latexmk.isEmpty()) {
        const auto choice = QMessageBox::question(this,
            QStringLiteral("LaTeX Not Found"),
            QStringLiteral("No usable LaTeX compiler was found. Open the MiKTeX download page?"),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::Yes);
        if (choice == QMessageBox::Yes) {
            QDesktopServices::openUrl(QUrl(LatexEnvironmentService::miktexDownloadUrl()));
        }
        return;
    }

    const auto choice = QMessageBox::question(this,
        QStringLiteral("Perl Required"),
        QStringLiteral("MiKTeX latexmk requires Perl. Install Strawberry Perl for full latexmk support, or configure pdflatex if available. Open Strawberry Perl download page?"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::Yes);
    if (choice == QMessageBox::Yes) {
        QDesktopServices::openUrl(QUrl(LatexEnvironmentService::perlDownloadUrl()));
    }
}

void MainWindow::openCompiledPdf()
{
    const auto pdfPath = currentPdfPath();
    if (pdfPath.isEmpty() || !QFileInfo::exists(pdfPath)) {
        QMessageBox::information(this, QStringLiteral("PDF Not Found"), QStringLiteral("Compile the project before opening the PDF."));
        return;
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(pdfPath));
}

void MainWindow::copyProject()
{
    if (m_projectService.projectRoot().isEmpty() || !m_standaloneFilePath.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("No Project"), QStringLiteral("Open a project before copying it."));
        return;
    }
    const auto destination = QFileDialog::getExistingDirectory(this, QStringLiteral("Copy Project To"));
    if (destination.isEmpty()) {
        return;
    }

    const auto sourceInfo = QFileInfo(m_projectService.projectRoot());
    const auto target = QDir(destination).filePath(sourceInfo.fileName());
    QString error;
    if (!copyDirectoryRecursively(m_projectService.projectRoot(), target, &error)) {
        QMessageBox::warning(this, QStringLiteral("Copy Failed"), error);
        return;
    }
    statusBar()->showMessage(QStringLiteral("Project copied to %1").arg(QDir::toNativeSeparators(target)), 5000);
}

void MainWindow::exportProjectZip()
{
    if (m_projectService.projectRoot().isEmpty() || !m_standaloneFilePath.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("No Project"), QStringLiteral("Open a project before exporting it."));
        return;
    }
    const auto defaultName = QFileInfo(m_projectService.projectRoot()).fileName() + QStringLiteral(".zip");
    const auto zipPath = QFileDialog::getSaveFileName(this, QStringLiteral("Export Project ZIP"), defaultName, QStringLiteral("ZIP archives (*.zip)"));
    if (zipPath.isEmpty()) {
        return;
    }

    const auto sourcePattern = QDir(m_projectService.projectRoot()).filePath(QStringLiteral("*"));
    const QString command = QStringLiteral("Compress-Archive -Path $args[0] -DestinationPath $args[1] -Force");
    const int exitCode = QProcess::execute(QStringLiteral("powershell.exe"), {
        QStringLiteral("-NoProfile"),
        QStringLiteral("-ExecutionPolicy"),
        QStringLiteral("Bypass"),
        QStringLiteral("-Command"),
        command,
        sourcePattern,
        zipPath,
    });

    if (exitCode != 0) {
        QMessageBox::warning(this, QStringLiteral("Export Failed"), QStringLiteral("PowerShell Compress-Archive failed."));
        return;
    }
    statusBar()->showMessage(QStringLiteral("Project exported to %1").arg(QDir::toNativeSeparators(zipPath)), 5000);
}

void MainWindow::showPreferences()
{
    SettingsDialog dialog(&m_environmentService, this);
    dialog.exec();
}

void MainWindow::onTreeActivated(const QModelIndex &index)
{
    const auto path = m_fileSystemModel.filePath(index);
    const QFileInfo info(path);
    if (!info.isFile()) {
        return;
    }
    if (info.suffix().compare(QStringLiteral("pdf"), Qt::CaseInsensitive) == 0) {
        loadPdf(path);
    } else {
        openFileInEditor(path);
    }
}

void MainWindow::onDiagnosticActivated(const QModelIndex &index)
{
    if (!index.isValid()) {
        return;
    }
    const auto &diagnostic = m_diagnosticsModel.diagnosticAt(index.row());
    if (!diagnostic.filePath.isEmpty()) {
        openFileInEditor(diagnostic.filePath, diagnostic.line);
    }
}

void MainWindow::onBuildStarted()
{
    m_buildOutput->clear();
    m_diagnosticsModel.clear();
    statusBar()->showMessage(m_currentBuildIsPreview
        ? QStringLiteral("Compiling live preview...")
        : QStringLiteral("Compiling..."));
}

void MainWindow::onBuildOutput(const QString &text)
{
    m_buildOutput->moveCursor(QTextCursor::End);
    m_buildOutput->insertPlainText(text);
    m_buildOutput->moveCursor(QTextCursor::End);
}

void MainWindow::onBuildFinished(bool success, const QString &pdfPath, QVector<Diagnostic> diagnostics)
{
    const auto wasPreview = m_currentBuildIsPreview;
    const auto runQueuedPreview = wasPreview && m_livePreviewQueued && m_liveCompileEnabled && !activeBuildRoot().isEmpty();
    m_diagnosticsModel.setDiagnostics(mapPreviewDiagnostics(std::move(diagnostics)));
    if (success) {
        loadPdf(pdfPath);
        statusBar()->showMessage(wasPreview ? QStringLiteral("Live preview updated") : QStringLiteral("Build finished"), 5000);
    } else {
        if (wasPreview) {
            m_pendingPreviewBuildRoot.reset();
        }
        statusBar()->showMessage(wasPreview ? QStringLiteral("Live preview failed") : QStringLiteral("Build failed"), 5000);
    }
    m_currentBuildIsPreview = false;
    if (runQueuedPreview) {
        m_autoCompileTimer.start();
    }
}
