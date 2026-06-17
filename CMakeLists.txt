cmake_minimum_required(VERSION 3.26)

project(LaTeXApp
    VERSION 1.0.0
    DESCRIPTION "Windows desktop LaTeX editor"
    LANGUAGES CXX
)

if(NOT WIN32)
    message(FATAL_ERROR "LaTeXApp v1 is Windows-only. Configure on Windows with MSVC.")
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/Zc:__cplusplus)
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 6.6 REQUIRED COMPONENTS Widgets Test)
find_package(Qt6 6.6 QUIET COMPONENTS Pdf PdfWidgets)

set(LATEXAPP_HAS_QTPDF OFF)
if(TARGET Qt6::Pdf AND TARGET Qt6::PdfWidgets)
    set(LATEXAPP_HAS_QTPDF ON)
endif()

qt_standard_project_setup()

set(LATEXAPP_SOURCES
    src/main.cpp
    src/models/Diagnostic.h
    src/models/DiagnosticsModel.cpp
    src/models/DiagnosticsModel.h
    src/models/ProjectConfig.cpp
    src/models/ProjectConfig.h
    src/models/ProjectTreeModel.cpp
    src/models/ProjectTreeModel.h
    src/services/BuildManager.cpp
    src/services/BuildManager.h
    src/services/LatexEnvironmentService.cpp
    src/services/LatexEnvironmentService.h
    src/services/ProjectService.cpp
    src/services/ProjectService.h
    src/services/SyncTexService.cpp
    src/services/SyncTexService.h
    src/services/TemplateService.cpp
    src/services/TemplateService.h
    src/ui/MainWindow.cpp
    src/ui/MainWindow.h
    src/ui/SettingsDialog.cpp
    src/ui/SettingsDialog.h
    src/widgets/LatexEditor.cpp
    src/widgets/LatexEditor.h
    src/widgets/LatexHighlighter.cpp
    src/widgets/LatexHighlighter.h
)

configure_file(
    src/resources/app.rc.in
    ${CMAKE_BINARY_DIR}/generated/latexapp.rc
    @ONLY
)

qt_add_executable(latexapp WIN32 ${LATEXAPP_SOURCES})

target_include_directories(latexapp PRIVATE src)

target_compile_definitions(latexapp PRIVATE
    LATEXAPP_VERSION="${PROJECT_VERSION}"
)

target_sources(latexapp PRIVATE
    ${CMAKE_BINARY_DIR}/generated/latexapp.rc
)

target_link_libraries(latexapp PRIVATE
    Qt6::Widgets
)

if(LATEXAPP_HAS_QTPDF)
    target_compile_definitions(latexapp PRIVATE LATEXAPP_HAS_QTPDF=1)
    target_sources(latexapp PRIVATE
        src/widgets/PdfPreviewView.cpp
        src/widgets/PdfPreviewView.h
    )
    target_link_libraries(latexapp PRIVATE Qt6::Pdf Qt6::PdfWidgets)
else()
    message(WARNING "Qt PDF modules were not found. Building without embedded PDF preview; users can still open compiled PDFs externally.")
endif()

set_target_properties(latexapp PROPERTIES
    OUTPUT_NAME "LaTeXApp"
    WIN32_EXECUTABLE TRUE
)

install(TARGETS latexapp
    BUNDLE DESTINATION .
    RUNTIME DESTINATION .
)
install(DIRECTORY templates DESTINATION .)

configure_file(
    installer/latexapp.iss.in
    ${CMAKE_BINARY_DIR}/installer/latexapp.iss
    @ONLY
)

include(CTest)
if(BUILD_TESTING)
    qt_add_executable(latexapp_tests
        tests/main.cpp
        src/models/Diagnostic.h
        src/models/ProjectConfig.h
        src/models/ProjectTreeModel.h
        src/services/BuildManager.h
        src/services/LatexEnvironmentService.h
        src/services/ProjectService.h
        src/services/SyncTexService.h
        src/services/TemplateService.h
        src/models/DiagnosticsModel.cpp
        src/models/ProjectConfig.cpp
        src/models/ProjectTreeModel.cpp
        src/services/BuildManager.cpp
        src/services/LatexEnvironmentService.cpp
        src/services/ProjectService.cpp
        src/services/SyncTexService.cpp
        src/services/TemplateService.cpp
    )
    target_include_directories(latexapp_tests PRIVATE src)
    target_link_libraries(latexapp_tests PRIVATE Qt6::Widgets Qt6::Test)
    add_test(NAME latexapp_tests COMMAND latexapp_tests)
endif()
