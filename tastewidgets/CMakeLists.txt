cmake_minimum_required(VERSION 3.18)
project(Traceability VERSION "0.0.1" LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 COMPONENTS Core Gui Network Test Widgets Concurrent Xml REQUIRED)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

add_definitions(-DQT_DEPRECATED_WARNINGS)

add_subdirectory(qgitlabapi)
add_subdirectory(QXlsx)
add_subdirectory(tracecommon)
add_subdirectory(requirements)
add_subdirectory(reviews)
