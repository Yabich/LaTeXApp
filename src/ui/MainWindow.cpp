#include "ui/MainWindow.h"

#include "services/TemplateService.h"
#include "ui/SettingsDialog.h"

#ifdef LATEXAPP_HAS_QTPDF
#include "widgets/PdfPreviewView.h"
#endif

#include <QAction>
#include <QApplication>
#include <QAbstractItemView>
#include <QCheckBox>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QSettings>
#include <QShortcut>
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
        compactBuildPanel();
        QTimer::singleShot(0, this, &MainWindow::scheduleAutoCompile);
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
    auto *editMenu = menuBar()->addMenu(QStringLiteral("&Edit"));
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

    addAction(editMenu, QStringLiteral("Find"), QKeySequence::Find, &MainWindow::showFindPanel);
    addAction(editMenu, QStringLiteral("Replace"), QKeySequence(QStringLiteral("Ctrl+H")), &MainWindow::showReplacePanel);

    addAction(projectMenu, QStringLiteral("Compile"), QKeySequence(QStringLiteral("Ctrl+R")), &MainWindow::compileProject);
    addAction(projectMenu, QStringLiteral("Open Compiled PDF"), QKeySequence(), &MainWindow::openCompiledPdf);
    addAction(projectMenu, QStringLiteral("Save PDF As..."), QKeySequence(), &MainWindow::savePdfAs);
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
    m_projectTreeModel.setSourceModel(&m_fileSystemModel);
    m_projectTree->setModel(&m_projectTreeModel);
    m_projectTree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_projectTree, &QTreeView::customContextMenuRequested, this, &MainWindow::showProjectTreeContextMenu);
    connect(m_projectTree, &QTreeView::doubleClicked, this, &MainWindow::onTreeActivated);

    m_editorTabs = new QTabWidget(this);
    m_editorTabs->setTabsClosable(true);
    m_editorTabs->setDocumentMode(true);
    connect(m_editorTabs, &QTabWidget::tabCloseRequested, this, [this](int index) {
        auto *editor = qobject_cast<LatexEditor *>(m_editorTabs->widget(index));
        if (!editor || !saveEditor(editor)) {
            return;
        }
        m_openEditors.remove(QFileInfo(editor->filePath()).canonicalFilePath());
        m_editorTabs->removeTab(index);
        editor->deleteLater();
    });
    connect(m_editorTabs, &QTabWidget::currentChanged, this, [this]() { updateSearchStatus(); });

#ifdef LATEXAPP_HAS_QTPDF
    auto *pdfPane = new PdfPreviewView(this);
    m_pdfView = pdfPane;
    m_pdfView->setDocument(&m_pdfDocument);
    m_pdfView->setPageMode(QPdfView::PageMode::MultiPage);
    m_pdfView->setZoomMode(QPdfView::ZoomMode::FitToWidth);
    connect(pdfPane, &PdfPreviewView::pdfSyncRequested, this, &MainWindow::syncPdfToSource);
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
    bottomTabs->setMinimumHeight(90);

    m_searchPanel = createSearchReplacePanel();
    auto *editorPane = new QWidget(this);
    auto *editorPaneLayout = new QVBoxLayout(editorPane);
    editorPaneLayout->setContentsMargins(0, 0, 0, 0);
    editorPaneLayout->setSpacing(0);
    editorPaneLayout->addWidget(m_searchPanel);
    editorPaneLayout->addWidget(m_editorTabs, 1);

    m_editorAndBottomSplitter = new QSplitter(Qt::Vertical, this);
    m_editorAndBottomSplitter->addWidget(editorPane);
    m_editorAndBottomSplitter->addWidget(bottomTabs);
    m_editorAndBottomSplitter->setStretchFactor(0, 8);
    m_editorAndBottomSplitter->setStretchFactor(1, 1);
    compactBuildPanel();

    auto *mainSplitter = new QSplitter(Qt::Horizontal, this);
    mainSplitter->addWidget(m_projectTree);
    mainSplitter->addWidget(m_editorAndBottomSplitter);
    mainSplitter->addWidget(pdfPane);
    mainSplitter->setStretchFactor(0, 1);
    mainSplitter->setStretchFactor(1, 4);
    mainSplitter->setStretchFactor(2, 3);

    return mainSplitter;
}

QWidget *MainWindow::createSearchReplacePanel()
{
    auto *panel = new QWidget(this);
    panel->setVisible(false);
    panel->setStyleSheet(QStringLiteral(
        "QWidget { background: #f8fafc; border-bottom: 1px solid #d8dee8; }"
        "QLineEdit { background: #ffffff; color: #111827; border: 1px solid #cfd6e2; border-radius: 4px; padding: 5px 7px; }"
        "QPushButton { background: #ffffff; color: #111827; border: 1px solid #cfd6e2; border-radius: 4px; padding: 5px 9px; }"
        "QPushButton:hover { background: #eef3f8; }"
        "QLabel { color: #4b5563; }"
        "QCheckBox { color: #374151; }"
    ));

    auto *layout = new QHBoxLayout(panel);
    layout->setContentsMargins(8, 6, 8, 6);
    layout->setSpacing(6);

    layout->addWidget(new QLabel(QStringLiteral("Find"), panel));
    m_searchText = new QLineEdit(panel);
    m_searchText->setClearButtonEnabled(true);
    m_searchText->setMinimumWidth(220);
    layout->addWidget(m_searchText);

    layout->addWidget(new QLabel(QStringLiteral("Replace"), panel));
    m_replaceText = new QLineEdit(panel);
    m_replaceText->setClearButtonEnabled(true);
    m_replaceText->setMinimumWidth(180);
    layout->addWidget(m_replaceText);

    m_matchCaseCheck = new QCheckBox(QStringLiteral("Match case"), panel);
    connect(m_matchCaseCheck, &QCheckBox::toggled, this, [this]() { updateSearchStatus(); });
    layout->addWidget(m_matchCaseCheck);

    auto *previousButton = new QPushButton(QStringLiteral("Previous"), panel);
    connect(previousButton, &QPushButton::clicked, this, &MainWindow::findPreviousMatch);
    layout->addWidget(previousButton);

    auto *nextButton = new QPushButton(QStringLiteral("Next"), panel);
    connect(nextButton, &QPushButton::clicked, this, &MainWindow::findNextMatch);
    layout->addWidget(nextButton);

    auto *replaceButton = new QPushButton(QStringLiteral("Replace"), panel);
    connect(replaceButton, &QPushButton::clicked, this, &MainWindow::replaceCurrentMatch);
    layout->addWidget(replaceButton);

    auto *replaceAllButton = new QPushButton(QStringLiteral("Replace All"), panel);
    connect(replaceAllButton, &QPushButton::clicked, this, &MainWindow::replaceAllMatches);
    layout->addWidget(replaceAllButton);

    m_searchStatus = new QLabel(panel);
    m_searchStatus->setMinimumWidth(96);
    layout->addWidget(m_searchStatus);
    layout->addStretch(1);

    auto *closeButton = new QPushButton(QStringLiteral("Close"), panel);
    connect(closeButton, &QPushButton::clicked, this, &MainWindow::hideSearchReplacePanel);
    layout->addWidget(closeButton);

    connect(m_searchText, &QLineEdit::returnPressed, this, &MainWindow::findNextMatch);
    connect(m_searchText, &QLineEdit::textChanged, this, [this]() { updateSearchStatus(); });
    connect(m_replaceText, &QLineEdit::returnPressed, this, &MainWindow::replaceCurrentMatch);

    auto *escapeShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), panel);
    escapeShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(escapeShortcut, &QShortcut::activated, this, &MainWindow::hideSearchReplacePanel);

    return panel;
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
    m_displayedPdfPath.clear();
    m_displayedBuildRoot.clear();
    m_displayedSourceRoot.clear();
    m_displayedPdfFromPreview = false;
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
    m_displayedPdfPath.clear();
    m_displayedBuildRoot.clear();
    m_displayedSourceRoot.clear();
    m_displayedPdfFromPreview = false;
    m_projectTree->setVisible(false);
    if (m_autoCompileAction) {
        QSignalBlocker blocker(m_autoCompileAction);
        m_autoCompileAction->setChecked(m_liveCompileEnabled);
    }
    updateWindowTitle();
    showWorkspacePage();
    compactBuildPanel();
    QTimer::singleShot(0, this, &MainWindow::scheduleAutoCompile);
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
    connect(editor, &LatexEditor::sourceSyncRequested, this, &MainWindow::syncSourceToPdf);

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

void MainWindow::compactBuildPanel()
{
    if (!m_editorAndBottomSplitter) {
        return;
    }

    m_editorAndBottomSplitter->setSizes({900, 115});
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
    const auto loadedPreview = m_currentBuildIsPreview;
    const auto loadedBuildRoot = loadedPreview ? m_previewMirrorRoot : activeBuildRoot();
    const auto loadedSourceRoot = loadedPreview ? m_previewSourceRoot : activeBuildRoot();
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
        m_displayedPdfPath = pdfPath;
        m_displayedBuildRoot = loadedBuildRoot;
        m_displayedSourceRoot = loadedSourceRoot;
        m_displayedPdfFromPreview = loadedPreview;
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
    m_displayedPdfPath = pdfPath;
    m_displayedBuildRoot = loadedBuildRoot;
    m_displayedSourceRoot = loadedSourceRoot;
    m_displayedPdfFromPreview = loadedPreview;
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

QString MainWindow::displayedPdfPath() const
{
    if (!m_displayedPdfPath.isEmpty() && QFileInfo::exists(m_displayedPdfPath)) {
        return m_displayedPdfPath;
    }
    const auto pdfPath = currentPdfPath();
    return QFileInfo::exists(pdfPath) ? pdfPath : QString();
}

QString MainWindow::mapSourcePathToDisplayedBuildPath(const QString &sourcePath) const
{
    const QFileInfo sourceInfo(sourcePath);
    const auto canonicalSourcePath = sourceInfo.exists() ? sourceInfo.canonicalFilePath() : sourceInfo.absoluteFilePath();
    if (!m_displayedPdfFromPreview || m_displayedSourceRoot.isEmpty() || m_displayedBuildRoot.isEmpty()) {
        return canonicalSourcePath;
    }

    const QDir sourceRoot(m_displayedSourceRoot);
    const auto relativePath = sourceRoot.relativeFilePath(canonicalSourcePath);
    if (relativePath.startsWith(QStringLiteral("..")) || QFileInfo(relativePath).isAbsolute()) {
        return canonicalSourcePath;
    }

    return QDir(m_displayedBuildRoot).filePath(relativePath);
}

QString MainWindow::mapDisplayedBuildPathToSourcePath(const QString &buildPath) const
{
    QFileInfo buildInfo(buildPath);
    const auto absoluteBuildPath = buildInfo.isRelative()
        ? QDir(m_displayedBuildRoot).filePath(buildPath)
        : (buildInfo.exists() ? buildInfo.canonicalFilePath() : buildInfo.absoluteFilePath());

    if (!m_displayedPdfFromPreview || m_displayedSourceRoot.isEmpty() || m_displayedBuildRoot.isEmpty()) {
        return absoluteBuildPath;
    }

    const QDir buildRoot(m_displayedBuildRoot);
    const auto relativePath = buildRoot.relativeFilePath(absoluteBuildPath);
    if (relativePath.startsWith(QStringLiteral("..")) || QFileInfo(relativePath).isAbsolute()) {
        return absoluteBuildPath;
    }

    return QDir(m_displayedSourceRoot).filePath(relativePath);
}

void MainWindow::syncSourceToPdf(const QString &sourcePath, int line, int column)
{
#ifdef LATEXAPP_HAS_QTPDF
    const auto pdfPath = displayedPdfPath();
    if (pdfPath.isEmpty()) {
        statusBar()->showMessage(QStringLiteral("Compile the document before using source/PDF sync."), 5000);
        return;
    }

    const auto buildSourcePath = mapSourcePathToDisplayedBuildPath(sourcePath);
    const auto result = m_syncTexService.forwardSearch(buildSourcePath, line, column, pdfPath);
    if (!result.success) {
        statusBar()->showMessage(result.message.isEmpty() ? QStringLiteral("No matching PDF position found.") : result.message, 6000);
        return;
    }

    if (m_pdfView && m_pdfView->pageNavigator()) {
        const auto targetPage = qBound(0, result.page - 1, qMax(0, m_pdfDocument.pageCount() - 1));
        m_pdfView->pageNavigator()->jump(targetPage, result.position);
        statusBar()->showMessage(QStringLiteral("Synced source line %1 to PDF").arg(line), 3000);
    }
#else
    Q_UNUSED(sourcePath)
    Q_UNUSED(line)
    Q_UNUSED(column)
    statusBar()->showMessage(QStringLiteral("Embedded PDF preview is not available."), 5000);
#endif
}

void MainWindow::syncPdfToSource(int oneBasedPage, const QPointF &pagePoint)
{
    const auto pdfPath = displayedPdfPath();
    if (pdfPath.isEmpty()) {
        statusBar()->showMessage(QStringLiteral("Compile the document before using PDF/source sync."), 5000);
        return;
    }

    const auto result = m_syncTexService.inverseSearch(pdfPath, oneBasedPage, pagePoint);
    if (!result.success) {
        statusBar()->showMessage(result.message.isEmpty() ? QStringLiteral("No matching source position found.") : result.message, 6000);
        return;
    }

    const auto sourcePath = mapDisplayedBuildPathToSourcePath(result.inputFile);
    if (openFileInEditor(sourcePath, result.line)) {
        statusBar()->showMessage(QStringLiteral("Synced PDF page %1 to source line %2").arg(oneBasedPage).arg(result.line), 3000);
    } else {
        statusBar()->showMessage(QStringLiteral("SyncTeX found a source file, but it could not be opened."), 6000);
    }
}

void MainWindow::setProjectRootInTree(const QString &projectRoot)
{
    const auto rootIndex = m_fileSystemModel.setRootPath(projectRoot);
    const auto config = m_projectService.config();
    m_projectTreeModel.setProjectMetadata(projectRoot, config.mainFile, config.outputDirectory);
    m_projectTree->setRootIndex(m_projectTreeModel.mapFromSource(rootIndex));
    for (int column = 1; column < m_projectTreeModel.columnCount(); ++column) {
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

QString MainWindow::projectTreePathFromIndex(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return {};
    }

    return m_fileSystemModel.filePath(m_projectTreeModel.mapToSource(index));
}

QString MainWindow::selectedProjectTreeDirectory(const QModelIndex &index) const
{
    const auto path = projectTreePathFromIndex(index);
    if (path.isEmpty()) {
        return m_projectService.projectRoot();
    }

    const QFileInfo info(path);
    return info.isDir() ? info.absoluteFilePath() : info.absolutePath();
}

bool MainWindow::isProjectItemPath(const QString &path) const
{
    if (path.isEmpty() || m_projectService.projectRoot().isEmpty() || !m_standaloneFilePath.isEmpty()) {
        return false;
    }

    const QFileInfo info(path);
    const QDir root(m_projectService.projectRoot());
    const auto relativePath = QDir::cleanPath(root.relativeFilePath(info.absoluteFilePath()));
    return relativePath != QStringLiteral(".")
        && relativePath != QStringLiteral("..")
        && !relativePath.startsWith(QStringLiteral("../"))
        && !QFileInfo(relativePath).isAbsolute();
}

bool MainWindow::isValidProjectTreeName(const QString &name) const
{
    const auto trimmed = name.trimmed();
    return !trimmed.isEmpty()
        && trimmed != QStringLiteral(".")
        && trimmed != QStringLiteral("..")
        && !trimmed.contains(QLatin1Char('/'))
        && !trimmed.contains(QLatin1Char('\\'));
}

void MainWindow::updateMainFileAfterPathChanged(const QString &oldPath, const QString &newPath)
{
    if (m_projectService.projectRoot().isEmpty() || !m_standaloneFilePath.isEmpty()) {
        return;
    }

    const QDir root(m_projectService.projectRoot());
    const auto config = m_projectService.config();
    const auto mainAbsolutePath = QFileInfo(root.filePath(config.mainFile)).absoluteFilePath();
    const QFileInfo oldInfo(oldPath);
    const auto oldAbsolutePath = oldInfo.absoluteFilePath();
    const auto mainIsInsideOldFolder = mainAbsolutePath.startsWith(oldAbsolutePath + QLatin1Char('/'));
    const auto oldIsFolder = oldInfo.isDir()
        || mainIsInsideOldFolder
        || (!newPath.isEmpty() && QFileInfo(newPath).isDir());
    const auto mainWasAffected = oldIsFolder
        ? (mainAbsolutePath == oldAbsolutePath || mainIsInsideOldFolder)
        : mainAbsolutePath == oldAbsolutePath;

    if (!mainWasAffected) {
        return;
    }

    auto updatedConfig = config;
    if (!newPath.isEmpty()) {
        const QFileInfo newInfo(newPath);
        if (oldIsFolder) {
            const auto relativeInsideFolder = QDir(oldAbsolutePath).relativeFilePath(mainAbsolutePath);
            updatedConfig.mainFile = QDir::cleanPath(root.relativeFilePath(QDir(newInfo.absoluteFilePath()).filePath(relativeInsideFolder)));
        } else {
            updatedConfig.mainFile = QDir::cleanPath(root.relativeFilePath(newInfo.absoluteFilePath()));
        }
    } else {
        updatedConfig.mainFile.clear();
        QDirIterator iterator(m_projectService.projectRoot(), {QStringLiteral("*.tex")}, QDir::Files, QDirIterator::Subdirectories);
        while (iterator.hasNext()) {
            const auto candidate = iterator.next();
            const auto relativeCandidate = QDir::cleanPath(root.relativeFilePath(candidate));
            if (relativeCandidate.startsWith(QStringLiteral(".latexapp/"))
                || (!config.outputDirectory.isEmpty() && relativeCandidate.startsWith(QDir::cleanPath(config.outputDirectory) + QLatin1Char('/')))) {
                continue;
            }
            updatedConfig.mainFile = relativeCandidate;
            break;
        }
    }

    QString error;
    m_projectService.setConfig(updatedConfig);
    if (!m_projectService.saveConfig(&error)) {
        QMessageBox::warning(this, QStringLiteral("Project Config"), error);
    }
    m_projectTreeModel.setProjectMetadata(m_projectService.projectRoot(), updatedConfig.mainFile, updatedConfig.outputDirectory);
}

void MainWindow::closeEditorForPath(const QString &path)
{
    const QFileInfo targetInfo(path);
    const auto targetPath = targetInfo.absoluteFilePath();
    const auto targetIsDir = targetInfo.isDir();

    for (int i = m_editorTabs->count() - 1; i >= 0; --i) {
        auto *editor = qobject_cast<LatexEditor *>(m_editorTabs->widget(i));
        if (!editor) {
            continue;
        }

        const auto editorPath = QFileInfo(editor->filePath()).absoluteFilePath();
        const auto affected = targetIsDir
            ? (editorPath == targetPath || editorPath.startsWith(targetPath + QLatin1Char('/')))
            : editorPath == targetPath;
        if (!affected) {
            continue;
        }

        m_openEditors.remove(QFileInfo(editor->filePath()).canonicalFilePath());
        m_editorTabs->removeTab(i);
        editor->deleteLater();
    }
}

void MainWindow::rekeyOpenEditor(const QString &oldPath, const QString &newPath)
{
    const QFileInfo oldInfo(oldPath);
    const auto oldCanonical = oldInfo.canonicalFilePath();
    const auto oldAbsolute = oldInfo.absoluteFilePath();
    const auto newAbsolute = QFileInfo(newPath).absoluteFilePath();

    LatexEditor *editor = nullptr;
    if (m_openEditors.contains(oldCanonical)) {
        editor = m_openEditors.take(oldCanonical).data();
    }
    if (!editor) {
        for (int i = 0; i < m_editorTabs->count(); ++i) {
            auto *candidate = qobject_cast<LatexEditor *>(m_editorTabs->widget(i));
            if (candidate && QFileInfo(candidate->filePath()).absoluteFilePath() == oldAbsolute) {
                editor = candidate;
                m_openEditors.remove(QFileInfo(candidate->filePath()).canonicalFilePath());
                break;
            }
        }
    }
    if (!editor) {
        return;
    }

    editor->setFilePath(newAbsolute);
    m_openEditors.insert(QFileInfo(newAbsolute).canonicalFilePath(), editor);
    const auto tabIndex = m_editorTabs->indexOf(editor);
    if (tabIndex >= 0) {
        m_editorTabs->setTabText(tabIndex, QFileInfo(newAbsolute).fileName());
    }
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

void MainWindow::savePdfAs()
{
    if (activeBuildRoot().isEmpty()) {
        QMessageBox::information(this, QStringLiteral("No File"), QStringLiteral("Open a LaTeX file or project before saving a PDF."));
        return;
    }

    if (m_buildManager.isRunning()) {
        QMessageBox::information(this, QStringLiteral("Build Running"), QStringLiteral("Wait for the current compile to finish, then save the PDF."));
        return;
    }

    const auto config = activeBuildConfig();
    const auto defaultName = QFileInfo(config.mainFile).completeBaseName() + QStringLiteral(".pdf");
    const auto defaultPath = QDir(activeBuildRoot()).filePath(defaultName);
    auto targetPath = QFileDialog::getSaveFileName(this,
        QStringLiteral("Save As PDF"),
        defaultPath,
        QStringLiteral("PDF files (*.pdf);;All files (*.*)"));
    if (targetPath.isEmpty()) {
        return;
    }
    if (QFileInfo(targetPath).suffix().isEmpty()) {
        targetPath += QStringLiteral(".pdf");
    }

    m_pendingPdfSavePath = targetPath;
    compileProject();
    if (!m_buildManager.isRunning()) {
        m_pendingPdfSavePath.clear();
    } else {
        statusBar()->showMessage(QStringLiteral("Compiling before saving PDF..."), 5000);
    }
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

void MainWindow::showFindPanel()
{
    if (!m_searchPanel || !m_searchText) {
        return;
    }

    if (auto *editor = currentEditor()) {
        const auto selectedText = editor->textCursor().selectedText();
        if (!selectedText.isEmpty() && !selectedText.contains(QChar::ParagraphSeparator)) {
            m_searchText->setText(selectedText);
        }
    }

    m_searchPanel->setVisible(true);
    m_searchText->setFocus();
    m_searchText->selectAll();
    updateSearchStatus();
}

void MainWindow::showReplacePanel()
{
    showFindPanel();
    if (m_replaceText) {
        m_replaceText->setFocus();
        m_replaceText->selectAll();
    }
}

void MainWindow::hideSearchReplacePanel()
{
    if (m_searchPanel) {
        m_searchPanel->hide();
    }
    if (auto *editor = currentEditor()) {
        editor->setFocus();
    }
}

void MainWindow::findNextMatch()
{
    findTextInCurrentEditor(false);
}

void MainWindow::findPreviousMatch()
{
    findTextInCurrentEditor(true);
}

void MainWindow::replaceCurrentMatch()
{
    auto *editor = currentEditor();
    if (!editor || !m_searchText || !m_replaceText || m_searchText->text().isEmpty()) {
        updateSearchStatus(QStringLiteral("No search text"));
        return;
    }

    auto cursor = editor->textCursor();
    const auto selectedText = cursor.selectedText();
    const auto searchText = m_searchText->text();
    const auto comparison = m_matchCaseCheck && m_matchCaseCheck->isChecked()
        ? Qt::CaseSensitive
        : Qt::CaseInsensitive;

    if (selectedText.compare(searchText, comparison) != 0 && !findTextInCurrentEditor(false)) {
        return;
    }

    cursor = editor->textCursor();
    if (cursor.selectedText().compare(searchText, comparison) == 0) {
        cursor.insertText(m_replaceText->text());
        editor->setTextCursor(cursor);
        findTextInCurrentEditor(false);
    }
}

void MainWindow::replaceAllMatches()
{
    auto *editor = currentEditor();
    if (!editor || !m_searchText || !m_replaceText || m_searchText->text().isEmpty()) {
        updateSearchStatus(QStringLiteral("No search text"));
        return;
    }

    QTextDocument::FindFlags flags;
    if (m_matchCaseCheck && m_matchCaseCheck->isChecked()) {
        flags |= QTextDocument::FindCaseSensitively;
    }

    const auto searchText = m_searchText->text();
    const auto replacementText = m_replaceText->text();
    QTextCursor editCursor(editor->document());
    QTextCursor cursor(editor->document());
    int replacements = 0;

    editCursor.beginEditBlock();
    while (true) {
        cursor = editor->document()->find(searchText, cursor, flags);
        if (cursor.isNull()) {
            break;
        }
        cursor.insertText(replacementText);
        ++replacements;
    }
    editCursor.endEditBlock();

    updateSearchStatus(QStringLiteral("%1 replaced").arg(replacements));
    editor->setFocus();
}

bool MainWindow::findTextInCurrentEditor(bool backward)
{
    auto *editor = currentEditor();
    if (!editor || !m_searchText || m_searchText->text().isEmpty()) {
        updateSearchStatus(QStringLiteral("No search text"));
        return false;
    }

    QTextDocument::FindFlags flags;
    if (backward) {
        flags |= QTextDocument::FindBackward;
    }
    if (m_matchCaseCheck && m_matchCaseCheck->isChecked()) {
        flags |= QTextDocument::FindCaseSensitively;
    }

    const auto searchText = m_searchText->text();
    const auto originalCursor = editor->textCursor();
    bool found = editor->find(searchText, flags);
    if (!found) {
        QTextCursor wrapCursor(editor->document());
        wrapCursor.movePosition(backward ? QTextCursor::End : QTextCursor::Start);
        editor->setTextCursor(wrapCursor);
        found = editor->find(searchText, flags);
    }

    if (!found) {
        editor->setTextCursor(originalCursor);
        updateSearchStatus(QStringLiteral("No matches"));
        return false;
    }

    updateSearchStatus(QStringLiteral("Match found"));
    return true;
}

void MainWindow::updateSearchStatus(const QString &message)
{
    if (!m_searchStatus) {
        return;
    }
    if (!message.isEmpty()) {
        m_searchStatus->setText(message);
        return;
    }
    if (!m_searchText || m_searchText->text().isEmpty()) {
        m_searchStatus->clear();
        return;
    }

    auto *editor = currentEditor();
    if (!editor) {
        m_searchStatus->setText(QStringLiteral("No editor"));
        return;
    }

    QTextDocument::FindFlags flags;
    if (m_matchCaseCheck && m_matchCaseCheck->isChecked()) {
        flags |= QTextDocument::FindCaseSensitively;
    }

    QTextCursor cursor(editor->document());
    int matches = 0;
    while (true) {
        cursor = editor->document()->find(m_searchText->text(), cursor, flags);
        if (cursor.isNull()) {
            break;
        }
        ++matches;
    }

    m_searchStatus->setText(matches == 1
        ? QStringLiteral("1 match")
        : QStringLiteral("%1 matches").arg(matches));
}

void MainWindow::showProjectTreeContextMenu(const QPoint &position)
{
    if (!m_projectTree || m_projectService.projectRoot().isEmpty() || !m_standaloneFilePath.isEmpty()) {
        return;
    }

    const auto index = m_projectTree->indexAt(position);
    const auto path = projectTreePathFromIndex(index);
    const QFileInfo info(path);
    const auto parentDirectory = selectedProjectTreeDirectory(index);

    QMenu menu(this);
    auto *newFileAction = menu.addAction(QStringLiteral("New File..."));
    connect(newFileAction, &QAction::triggered, this, [this, parentDirectory]() { newFileInProjectTree(parentDirectory); });

    auto *newFolderAction = menu.addAction(QStringLiteral("New Folder..."));
    connect(newFolderAction, &QAction::triggered, this, [this, parentDirectory]() { newFolderInProjectTree(parentDirectory); });

    menu.addSeparator();
    if (info.exists() && info.isFile()) {
        if (info.suffix().compare(QStringLiteral("pdf"), Qt::CaseInsensitive) == 0) {
            auto *previewAction = menu.addAction(QStringLiteral("Open in Preview"));
            connect(previewAction, &QAction::triggered, this, [this, path]() { loadPdf(path); });

            auto *externalAction = menu.addAction(QStringLiteral("Open Externally"));
            connect(externalAction, &QAction::triggered, this, [path]() { QDesktopServices::openUrl(QUrl::fromLocalFile(path)); });
        } else {
            auto *openAction = menu.addAction(QStringLiteral("Open"));
            connect(openAction, &QAction::triggered, this, [this, path]() { openFileInEditor(path); });
        }

        if (info.suffix().compare(QStringLiteral("tex"), Qt::CaseInsensitive) == 0) {
            auto *mainFileAction = menu.addAction(QStringLiteral("Set as Main File"));
            connect(mainFileAction, &QAction::triggered, this, [this, path]() { setMainFileFromTree(path); });
        }

        menu.addSeparator();
        auto *renameAction = menu.addAction(QStringLiteral("Rename..."));
        connect(renameAction, &QAction::triggered, this, [this, path]() { renameProjectTreeItem(path); });

        auto *deleteAction = menu.addAction(QStringLiteral("Move to Recycle Bin..."));
        connect(deleteAction, &QAction::triggered, this, [this, path]() { deleteProjectTreeItem(path); });
    } else if (info.exists() && info.isDir() && isProjectItemPath(path)) {
        auto *renameAction = menu.addAction(QStringLiteral("Rename..."));
        connect(renameAction, &QAction::triggered, this, [this, path]() { renameProjectTreeItem(path); });

        auto *deleteAction = menu.addAction(QStringLiteral("Move to Recycle Bin..."));
        connect(deleteAction, &QAction::triggered, this, [this, path]() { deleteProjectTreeItem(path); });
    }

    menu.addSeparator();
    auto *revealAction = menu.addAction(QStringLiteral("Reveal in Explorer"));
    connect(revealAction, &QAction::triggered, this, [this, path, parentDirectory]() {
        revealProjectTreeItem(path.isEmpty() ? parentDirectory : path);
    });

    auto *refreshAction = menu.addAction(QStringLiteral("Refresh"));
    connect(refreshAction, &QAction::triggered, this, &MainWindow::refreshProjectTree);

    menu.exec(m_projectTree->viewport()->mapToGlobal(position));
}

void MainWindow::newFileInProjectTree(const QString &parentDirectory)
{
    if (!isProjectItemPath(parentDirectory) && QFileInfo(parentDirectory).absoluteFilePath() != QFileInfo(m_projectService.projectRoot()).absoluteFilePath()) {
        return;
    }

    bool ok = false;
    const auto name = QInputDialog::getText(this, QStringLiteral("New File"), QStringLiteral("File name"), QLineEdit::Normal, QStringLiteral("untitled.tex"), &ok).trimmed();
    if (!ok) {
        return;
    }
    if (!isValidProjectTreeName(name)) {
        QMessageBox::warning(this, QStringLiteral("New File"), QStringLiteral("Enter a file name without path separators."));
        return;
    }

    const auto filePath = QDir(parentDirectory).filePath(name);
    if (QFileInfo::exists(filePath)) {
        QMessageBox::warning(this, QStringLiteral("New File"), QStringLiteral("A file or folder with that name already exists."));
        return;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, QStringLiteral("New File"), file.errorString());
        return;
    }
    file.close();
    openFileInEditor(filePath);
    refreshProjectTree();
}

void MainWindow::newFolderInProjectTree(const QString &parentDirectory)
{
    if (!isProjectItemPath(parentDirectory) && QFileInfo(parentDirectory).absoluteFilePath() != QFileInfo(m_projectService.projectRoot()).absoluteFilePath()) {
        return;
    }

    bool ok = false;
    const auto name = QInputDialog::getText(this, QStringLiteral("New Folder"), QStringLiteral("Folder name"), QLineEdit::Normal, QStringLiteral("New Folder"), &ok).trimmed();
    if (!ok) {
        return;
    }
    if (!isValidProjectTreeName(name)) {
        QMessageBox::warning(this, QStringLiteral("New Folder"), QStringLiteral("Enter a folder name without path separators."));
        return;
    }

    const auto folderPath = QDir(parentDirectory).filePath(name);
    if (QFileInfo::exists(folderPath)) {
        QMessageBox::warning(this, QStringLiteral("New Folder"), QStringLiteral("A file or folder with that name already exists."));
        return;
    }
    if (!QDir(parentDirectory).mkdir(name)) {
        QMessageBox::warning(this, QStringLiteral("New Folder"), QStringLiteral("Could not create the folder."));
        return;
    }
    refreshProjectTree();
}

void MainWindow::renameProjectTreeItem(const QString &path)
{
    const QFileInfo info(path);
    if (!info.exists() || !isProjectItemPath(path)) {
        return;
    }

    bool ok = false;
    const auto newName = QInputDialog::getText(this, QStringLiteral("Rename"), QStringLiteral("New name"), QLineEdit::Normal, info.fileName(), &ok).trimmed();
    if (!ok || newName == info.fileName()) {
        return;
    }
    if (!isValidProjectTreeName(newName)) {
        QMessageBox::warning(this, QStringLiteral("Rename"), QStringLiteral("Enter a name without path separators."));
        return;
    }

    const auto newPath = QDir(info.absolutePath()).filePath(newName);
    if (QFileInfo::exists(newPath)) {
        QMessageBox::warning(this, QStringLiteral("Rename"), QStringLiteral("A file or folder with that name already exists."));
        return;
    }

    QVector<QPair<QString, QString>> editorMoves;
    const auto oldAbsolute = info.absoluteFilePath();
    for (int i = 0; i < m_editorTabs->count(); ++i) {
        auto *editor = qobject_cast<LatexEditor *>(m_editorTabs->widget(i));
        if (!editor) {
            continue;
        }

        const auto editorPath = QFileInfo(editor->filePath()).absoluteFilePath();
        const auto affected = info.isDir()
            ? (editorPath == oldAbsolute || editorPath.startsWith(oldAbsolute + QLatin1Char('/')))
            : editorPath == oldAbsolute;
        if (!affected) {
            continue;
        }
        if (!saveEditor(editor)) {
            return;
        }

        const auto movedPath = info.isDir()
            ? QDir(newPath).filePath(QDir(oldAbsolute).relativeFilePath(editorPath))
            : newPath;
        editorMoves.append({editorPath, movedPath});
    }

    if (!QDir(info.absolutePath()).rename(info.fileName(), newName)) {
        QMessageBox::warning(this, QStringLiteral("Rename"), QStringLiteral("Could not rename the selected item."));
        return;
    }

    for (const auto &move : editorMoves) {
        rekeyOpenEditor(move.first, move.second);
    }
    updateMainFileAfterPathChanged(path, newPath);
    refreshProjectTree();
    statusBar()->showMessage(QStringLiteral("Renamed %1").arg(info.fileName()), 3000);
}

void MainWindow::deleteProjectTreeItem(const QString &path)
{
    const QFileInfo info(path);
    if (!info.exists() || !isProjectItemPath(path)) {
        return;
    }

    const auto choice = QMessageBox::question(this,
        QStringLiteral("Move to Recycle Bin"),
        QStringLiteral("Move \"%1\" to the Recycle Bin?").arg(info.fileName()),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    if (choice != QMessageBox::Yes) {
        return;
    }

    for (int i = 0; i < m_editorTabs->count(); ++i) {
        auto *editor = qobject_cast<LatexEditor *>(m_editorTabs->widget(i));
        if (!editor) {
            continue;
        }

        const auto editorPath = QFileInfo(editor->filePath()).absoluteFilePath();
        const auto targetPath = info.absoluteFilePath();
        const auto affected = info.isDir()
            ? (editorPath == targetPath || editorPath.startsWith(targetPath + QLatin1Char('/')))
            : editorPath == targetPath;
        if (affected && !saveEditor(editor)) {
            return;
        }
    }

    if (!QFile::moveToTrash(path)) {
        QMessageBox::warning(this, QStringLiteral("Move to Recycle Bin"), QStringLiteral("Could not move the selected item to the Recycle Bin."));
        return;
    }

    closeEditorForPath(path);
    updateMainFileAfterPathChanged(path, {});
    refreshProjectTree();
    statusBar()->showMessage(QStringLiteral("Moved %1 to the Recycle Bin").arg(info.fileName()), 4000);
}

void MainWindow::revealProjectTreeItem(const QString &path)
{
    if (path.isEmpty()) {
        return;
    }

    const QFileInfo info(path);
    if (info.exists() && info.isFile()) {
        QProcess::startDetached(QStringLiteral("explorer.exe"), {QStringLiteral("/select,"), QDir::toNativeSeparators(info.absoluteFilePath())});
        return;
    }

    const auto folderPath = info.exists() && info.isDir() ? info.absoluteFilePath() : info.absolutePath();
    QDesktopServices::openUrl(QUrl::fromLocalFile(folderPath));
}

void MainWindow::setMainFileFromTree(const QString &texPath)
{
    const QFileInfo info(texPath);
    if (!info.exists()
        || !info.isFile()
        || info.suffix().compare(QStringLiteral("tex"), Qt::CaseInsensitive) != 0
        || !isProjectItemPath(texPath)) {
        return;
    }

    auto config = m_projectService.config();
    config.mainFile = QDir::cleanPath(QDir(m_projectService.projectRoot()).relativeFilePath(info.absoluteFilePath()));
    QString error;
    m_projectService.setConfig(config);
    if (!m_projectService.saveConfig(&error)) {
        QMessageBox::warning(this, QStringLiteral("Set Main File"), error);
        return;
    }

    m_projectTreeModel.setProjectMetadata(m_projectService.projectRoot(), config.mainFile, config.outputDirectory);
    statusBar()->showMessage(QStringLiteral("Main file set to %1").arg(config.mainFile), 4000);
}

void MainWindow::refreshProjectTree()
{
    if (m_projectService.projectRoot().isEmpty()) {
        return;
    }

    const auto config = m_projectService.config();
    const auto rootIndex = m_fileSystemModel.setRootPath(m_projectService.projectRoot());
    m_projectTreeModel.setProjectMetadata(m_projectService.projectRoot(), config.mainFile, config.outputDirectory);
    m_projectTree->setRootIndex(m_projectTreeModel.mapFromSource(rootIndex));
}

void MainWindow::onTreeActivated(const QModelIndex &index)
{
    const auto path = projectTreePathFromIndex(index);
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
        if (openFileInEditor(diagnostic.filePath, diagnostic.line) && diagnostic.line > 0) {
            syncSourceToPdf(diagnostic.filePath, diagnostic.line, 1);
        }
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
    const auto pendingPdfSavePath = wasPreview ? QString() : m_pendingPdfSavePath;
    if (!wasPreview) {
        m_pendingPdfSavePath.clear();
    }
    m_diagnosticsModel.setDiagnostics(mapPreviewDiagnostics(std::move(diagnostics)));
    if (success) {
        loadPdf(pdfPath);
        if (!pendingPdfSavePath.isEmpty()) {
            const QFileInfo sourceInfo(pdfPath);
            const QFileInfo targetInfo(pendingPdfSavePath);
            QDir targetDir = targetInfo.absoluteDir();
            if (!sourceInfo.exists()) {
                QMessageBox::warning(this, QStringLiteral("Save PDF Failed"), QStringLiteral("The compiler finished, but the PDF file was not found."));
                statusBar()->showMessage(QStringLiteral("PDF save failed"), 5000);
            } else if (sourceInfo.absoluteFilePath() == targetInfo.absoluteFilePath()) {
                statusBar()->showMessage(QStringLiteral("PDF saved to %1").arg(QDir::toNativeSeparators(pendingPdfSavePath)), 6000);
            } else if ((!targetDir.exists() && !targetDir.mkpath(QStringLiteral(".")))
                || (targetInfo.exists() && !QFile::remove(pendingPdfSavePath))
                || !QFile::copy(pdfPath, pendingPdfSavePath)) {
                QMessageBox::warning(this, QStringLiteral("Save PDF Failed"), QStringLiteral("Could not write the selected PDF file."));
                statusBar()->showMessage(QStringLiteral("PDF save failed"), 5000);
            } else {
                statusBar()->showMessage(QStringLiteral("PDF saved to %1").arg(QDir::toNativeSeparators(pendingPdfSavePath)), 6000);
            }
        } else {
            statusBar()->showMessage(wasPreview ? QStringLiteral("Live preview updated") : QStringLiteral("Build finished"), 5000);
        }
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
