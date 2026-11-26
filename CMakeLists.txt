cmake_minimum_required(VERSION 3.21)
project(webmconverter-qt VERSION 0.4.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets)

# ---------------------------------------------------------------------------
# Qt-free core: the "filter state -> ffmpeg argv" compiler + helpers.
# Kept free of Qt so it is unit-testable with a plain C++ compiler.
# ---------------------------------------------------------------------------
add_library(wc_core STATIC
  src/core/easing.cpp
  src/core/sizecalc.cpp
  src/core/progressparser.cpp
  src/core/commandbuilder.cpp
  src/core/previewfilter.cpp
)
target_include_directories(wc_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_features(wc_core PUBLIC cxx_std_17)

# ---------------------------------------------------------------------------
# Application
# ---------------------------------------------------------------------------
set(APP_SOURCES
  src/main.cpp

  # Qt IO adapters
  src/core/mediaprobe.cpp
  src/core/encoder.cpp
  src/core/ytdlprunner.cpp

  # Preview subsystem
  src/preview/framecache.cpp
  src/preview/previewengine.cpp

  # UI shell
  src/ui/mainwindow.cpp
  src/ui/previewcanvas.cpp
  src/ui/timelinewidget.cpp
  src/ui/consolepanel.cpp
  src/ui/encodingpanel.cpp
  src/ui/filterpanel.cpp
  src/ui/scrubpreview.cpp
  src/ui/appliedfilterspanel.cpp

  # Filter dialogs
  src/ui/dialogs/trimdialog.cpp
  src/ui/dialogs/multitrimdialog.cpp
  src/ui/dialogs/cropdialog.cpp
  src/ui/dialogs/resizedialog.cpp
  src/ui/dialogs/rotatedialog.cpp
  src/ui/dialogs/ratedialog.cpp
  src/ui/dialogs/dynamicratedialog.cpp
  src/ui/dialogs/captiondialog.cpp
  src/ui/dialogs/subtitledialog.cpp
  src/ui/dialogs/overlaydialog.cpp
  src/ui/dialogs/fadedialog.cpp
  src/ui/dialogs/eqdialog.cpp
  src/ui/dialogs/denoisedialog.cpp
  src/ui/dialogs/deinterlacedialog.cpp
  src/ui/dialogs/stabilizationdialog.cpp
  src/ui/dialogs/delayaudiodialog.cpp
)

set(APP_HEADERS
  src/core/mediaprobe.h
  src/core/encoder.h
  src/core/ytdlprunner.h
  src/preview/framecache.h
  src/preview/previewengine.h
  src/ui/mainwindow.h
  src/ui/previewcanvas.h
  src/ui/timelinewidget.h
  src/ui/consolepanel.h
  src/ui/encodingpanel.h
  src/ui/filterpanel.h
  src/ui/scrubpreview.h
  src/ui/appliedfilterspanel.h
  src/ui/dialogs/trimdialog.h
  src/ui/dialogs/multitrimdialog.h
  src/ui/dialogs/cropdialog.h
  src/ui/dialogs/resizedialog.h
  src/ui/dialogs/rotatedialog.h
  src/ui/dialogs/ratedialog.h
  src/ui/dialogs/dynamicratedialog.h
  src/ui/dialogs/captiondialog.h
  src/ui/dialogs/subtitledialog.h
  src/ui/dialogs/overlaydialog.h
  src/ui/dialogs/fadedialog.h
  src/ui/dialogs/eqdialog.h
  src/ui/dialogs/denoisedialog.h
  src/ui/dialogs/deinterlacedialog.h
  src/ui/dialogs/stabilizationdialog.h
  src/ui/dialogs/delayaudiodialog.h
)

add_executable(webmconverter-qt ${APP_SOURCES} ${APP_HEADERS})
target_include_directories(webmconverter-qt PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(webmconverter-qt PRIVATE wc_core Qt6::Widgets Qt6::Gui Qt6::Core)

if(MSVC)
  target_compile_options(webmconverter-qt PRIVATE /W3)
else()
  target_compile_options(webmconverter-qt PRIVATE -Wall -Wextra)
endif()

# ---------------------------------------------------------------------------
# Tests (Qt-free core)
# ---------------------------------------------------------------------------
enable_testing()
add_executable(core_tests tests/core_tests.cpp)
target_link_libraries(core_tests PRIVATE wc_core)
add_test(NAME core_tests COMMAND core_tests)

# ---------------------------------------------------------------------------
# Install (Linux packaging / AppImage reuses this)
# ---------------------------------------------------------------------------
include(GNUInstallDirs)
install(TARGETS webmconverter-qt RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/packaging/webmconverter-qt.desktop)
  install(FILES packaging/webmconverter-qt.desktop
          DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
endif()
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/packaging/io.github.webmforlazies.webmconverter.svg)
  install(FILES packaging/io.github.webmforlazies.webmconverter.svg
          DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
endif()
