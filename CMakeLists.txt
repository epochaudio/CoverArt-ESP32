# Build one board:  idf.py -B build/<board> -D COVERART_BOARD=<board> build
# Boards live in boards/<board>/ (see docs/adding-a-board.md).
cmake_minimum_required(VERSION 3.22)

if(NOT COVERART_BOARD)
    message(FATAL_ERROR "Choose a board: -D COVERART_BOARD=<name>, one of boards/*/")
endif()
set(COVERART_BOARD_DIR "${CMAKE_CURRENT_LIST_DIR}/boards/${COVERART_BOARD}")
if(NOT EXISTS "${COVERART_BOARD_DIR}/board.cmake")
    message(FATAL_ERROR "Unknown board '${COVERART_BOARD}': ${COVERART_BOARD_DIR}/board.cmake not found")
endif()
include("${COVERART_BOARD_DIR}/board.cmake")

set(IDF_TARGET ${COVERART_TARGET})
set(SDKCONFIG_DEFAULTS "sdkconfig.defaults;${COVERART_BOARD_DIR}/sdkconfig.defaults")
# Per-board config, so builds of different boards never mix.
set(SDKCONFIG "${CMAKE_BINARY_DIR}/sdkconfig")
# The board's component is always named "board".
set(EXTRA_COMPONENT_DIRS "${COVERART_BOARD_DIR}/board")
file(STRINGS "${CMAKE_CURRENT_LIST_DIR}/version.txt" PROJECT_VER LIMIT_COUNT 1)
# Re-run configuration when the version changes, so the new number is built in.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/version.txt")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
# One component lock file per board: boards pull different components.
idf_build_set_property(DEPENDENCIES_LOCK "${COVERART_BOARD_DIR}/dependencies.lock")
project(coverart)
