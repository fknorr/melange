cmake_minimum_required(VERSION 3.4 FATAL_ERROR)
project(melange)

set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
include(AppendArgs)

if (NOT CMAKE_BUILD_TYPE)
    message(STATUS "No build type specified, defaulting to debug build")
    set(CMAKE_BUILD_TYPE Debug)
endif()

append_args(CMAKE_C_FLAGS
    -fmax-errors=5
    -Wall
    -Wconversion
    -Wdouble-promotion
    -Wextra
    -Wpedantic
    -Wsign-conversion
    -Wstrict-overflow=4
    -Wsuggest-attribute=pure
    -Wsuggest-attribute=const
    -Wsuggest-attribute=noreturn
    -Wswitch-default
    -Wuninitialized
)

find_package(Gtk3 REQUIRED)
find_package(WebKit2Gtk REQUIRED)

include_directories(
    ${GTK3_INCLUDE_DIRS}
    ${WEBKIT2GTK_INCLUDE_DIRS}
)

link_directories(
    ${GTK3_LIBRARY_DIRS}
    ${WEBKIT2GTK_LIBRARY_DIRS}
)

add_executable(melange
    src/main.c
        src/app.c src/app.h src/mainwindow.c src/mainwindow.h)

target_link_libraries(melange
    ${GTK3_LIBRARIES}
    ${WEBKIT2GTK_LIBRARIES}
)

