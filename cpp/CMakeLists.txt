# SPDX-License-Identifier: GPL-3.0-or-later
cmake_minimum_required(VERSION 3.20)
project(Musubi VERSION 1.0.5 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC OFF)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Widgets)
include(CTest)

qt_add_executable(Musubi WIN32
    main.cpp
    chapter_parser.cpp
    chapter_parser.h
    muxer_window.cpp
    muxer_window.h
    mux_worker.cpp
    mux_worker.h
    resources/musubi.qrc
)

if(WIN32)
    target_sources(Musubi PRIVATE resources/musubi.rc)
endif()

target_link_libraries(Musubi PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets)
target_compile_definitions(Musubi PRIVATE QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII)

if(MSVC)
    target_compile_options(Musubi PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(Musubi PRIVATE -Wall -Wextra -Wpedantic)
endif()

install(TARGETS Musubi RUNTIME DESTINATION .)
if(UNIX AND NOT APPLE)
    install(FILES resources/linux/musubi.desktop DESTINATION share/applications)
    foreach(icon_size 16 32 48 64 128 256 512)
        install(FILES resources/icons/png/${icon_size}x${icon_size}/musubi.png
            DESTINATION share/icons/hicolor/${icon_size}x${icon_size}/apps RENAME musubi.png)
    endforeach()
endif()

if(BUILD_TESTING)
    add_executable(chapter_parser_tests
        chapter_parser_tests.cpp
        chapter_parser.cpp
        chapter_parser.h
        mux_worker.cpp
        mux_worker.h
    )
    target_link_libraries(chapter_parser_tests PRIVATE Qt6::Core)
    add_test(NAME chapter_parser_tests COMMAND chapter_parser_tests)
endif()
