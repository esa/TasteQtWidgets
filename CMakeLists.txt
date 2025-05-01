cmake_minimum_required(VERSION 3.16)

# Read the version information from the "VERSION" file
file(READ ${CMAKE_SOURCE_DIR}/VERSION VERSION_STRING)
string(STRIP "${VERSION_STRING}" VERSION_STRING)

project(TasteQtWidgets LANGUAGES CXX)
set(PROJECT_VERSION ${VERSION_STRING})
message(STATUS "TasteQtWidgets VERSION: <${PROJECT_VERSION}>")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network)

include(CCache)


add_subdirectory(tastewidgets)
