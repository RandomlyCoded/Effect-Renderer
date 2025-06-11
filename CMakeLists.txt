cmake_minimum_required(VERSION 3.16)

project(EffectRenderer VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Gui Widgets Multimedia MultimediaWidgets)

set(PROJECT_SOURCES
        src/main.cpp
        src/previewwindow.h src/previewwindow.cpp
        src/renderer.h src/renderer.cpp
        src/recorder.h src/recorder.cpp
)

qt_add_executable(EffectRenderer
    MANUAL_FINALIZATION
    ${PROJECT_SOURCES}
    PerlinNoise/perlinnoise.h PerlinNoise/perlinnoise.cpp
)

add_custom_target(Documentation SOURCES
    ReadMe.md
)

target_compile_definitions(EffectRenderer
    PUBLIC RANDOMLY_EXECUTABLE_NAME="${PROJECT_NAME}"
    PUBLIC RANDOMLY_VERSION="${PROJECT_VERSION}"
)

target_link_libraries(EffectRenderer PRIVATE Qt6::Gui Qt6::Widgets Qt6::Multimedia Qt6::MultimediaWidgets)

set_target_properties(EffectRenderer PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS EffectRenderer
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(EffectRenderer)
endif()
