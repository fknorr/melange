cmake_minimum_required(VERSION 3.4 FATAL_ERROR)
project(melange VERSION 0.1)

set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
include(AppendArgs)

if (NOT CMAKE_BUILD_TYPE)
    message(STATUS "No build type specified, defaulting to debug build")
    set(CMAKE_BUILD_TYPE Debug)
endif ()

find_package(Gtk3 REQUIRED)
find_package(WebKit2Gtk REQUIRED)
find_package(LibNotify REQUIRED)
find_package(FLEX REQUIRED)
find_package(BISON REQUIRED)

add_definitions(
    -DMELANGE_INSTALL_PREFIX="${CMAKE_INSTALL_PREFIX}"
    -DMELANGE_VERSION="${PROJECT_VERSION}"
)

append_args(
    CMAKE_C_FLAGS
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
    -Wuninitialized
)

include_directories(
    ${CMAKE_SOURCE_DIR}
    ${GTK3_INCLUDE_DIRS}
    ${WEBKIT2GTK_INCLUDE_DIRS}
    ${LIBNOTIFY_INCLUDE_DIRS}
)

link_directories(
    ${GTK3_LIBRARY_DIRS}
    ${WEBKIT2GTK_LIBRARY_DIRS}
    ${LIBNOTIFY_LIBRARY_DIRS}
)

flex_target(config_parser src/config.l ${CMAKE_CURRENT_BINARY_DIR}/config.c)
bison_target(config_parser src/config.y ${CMAKE_CURRENT_BINARY_DIR}/config.tab.c)

add_executable(
    melange
    src/main.c
    src/app.c src/app.h
    src/mainwindow.c src/mainwindow.h
    src/util.c src/util.h
    src/config.h src/config.c
    src/presets.c src/presets.h
    ${FLEX_config_parser_OUTPUTS}
    ${BISON_config_parser_OUTPUTS}
)

target_link_libraries(
    melange
    ${GTK3_LIBRARIES}
    ${WEBKIT2GTK_LIBRARIES}
    ${LIBNOTIFY_LIBRARIES}
)

set_target_properties(
    melange PROPERTIES LINK_FLAGS
    -rdynamic
)

configure_file(src/melange.desktop.in melange.desktop)

install(TARGETS melange RUNTIME DESTINATION bin)
install(DIRECTORY res/ DESTINATION share/melange)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/melange.desktop DESTINATION share/applications)
install(FILES res/icons/melange.svg DESTINATION share/icons/hicolor/scalable/apps)
