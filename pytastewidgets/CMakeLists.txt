cmake_minimum_required(VERSION 3.16)

# Read the version information from the "VERSION" file
file(READ ${CMAKE_CURRENT_SOURCE_DIR}/../VERSION VERSION_STRING)
string(STRIP "${VERSION_STRING}" VERSION_STRING)

project(TasteQtWidgets VERSION ${VERSION_STRING} DESCRIPTION "C++/Qt Widgets for ESA Python projects" LANGUAGES CXX)

message(STATUS "TasteQtWidgets VERSION: <${PROJECT_VERSION}>")
# Write version for python module
configure_file(__init__.py.in ${CMAKE_SOURCE_DIR}/__init__.py @ONLY)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake")

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network)
set(QT_VER_MAJ_MIN ${Qt6_VERSION_MAJOR}.${Qt6_VERSION_MINOR})
include(CCache)

set(Python3_FIND_VIRTUALENV FIRST)
find_package(Python3 REQUIRED COMPONENTS Interpreter Development)

execute_process(
    COMMAND
        ${Python3_EXECUTABLE} -c "import sysconfig; print(sysconfig.get_paths()['purelib'])"
    OUTPUT_VARIABLE PYTHON_LIBS_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

if(NOT DEFINED SHIBOKEN_CUSTOM_PREFIX) # look for shiboken in a custom location
    # TODO: ensure the PYTHON_LIBS_DIR is correct
    set(SHIBOKEN_CUSTOM_PREFIX "${PYTHON_LIBS_DIR}/shiboken6")
endif()

if(NOT DEFINED PYSIDE_CUSTOM_PREFIX) # look for pyside in a custom location
    # TODO: ensure the PYTHON_LIBS_DIR is correct
    set(PYSIDE_CUSTOM_PREFIX "${PYTHON_LIBS_DIR}/PySide6")
endif()

find_package(Shiboken6 ${QT_VER_MAJ_MIN} REQUIRED)
find_package(PySide6 ${QT_VER_MAJ_MIN} REQUIRED)

set(Python3_VERSION_MAJORMINOR "${Python3_VERSION_MAJOR}.${Python3_VERSION_MINOR}")
if(NOT TasteQtWidgets_PYTHON_BINDINGS_INSTALL_PREFIX)
    set(PYTHON_SITE_DIR $ENV{HOME}/.local/lib/python${Python3_VERSION_MAJORMINOR}/site-packages)
    if (EXISTS ${PYTHON_SITE_DIR})
        set(TasteQtWidgets_PYTHON_BINDINGS_INSTALL_PREFIX
            ${PYTHON_SITE_DIR}
            CACHE FILEPATH "Custom path to install python bindings."
        )
    else()
        set(TasteQtWidgets_PYTHON_BINDINGS_INSTALL_PREFIX
                ${PYTHON_LIBS_DIR}
                CACHE FILEPATH "Custom path to install python bindings."
            )
    endif()
endif()

set(TASTE_PYTHON_MODULE_NAME "Py${PROJECT_NAME}")
set(TasteQtWidgets_PYTHON_BINDINGS_INSTALL_PREFIX "${TasteQtWidgets_PYTHON_BINDINGS_INSTALL_PREFIX}/${TASTE_PYTHON_MODULE_NAME}")

include(KDPySide6ModuleBuild)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(original_library "QtTASTE")
set(bindings_library ${PROJECT_NAME})

set(TASTE_SUBLIB_SRC_ROOT ${CMAKE_SOURCE_DIR}/../tastewidgets)
add_subdirectory(${TASTE_SUBLIB_SRC_ROOT}/qgitlabapi ${CMAKE_BINARY_DIR}/tastewidgets/qgitlabapi)
add_subdirectory(${TASTE_SUBLIB_SRC_ROOT}/tracecommon ${CMAKE_BINARY_DIR}/tastewidgets/tracecommon)
add_subdirectory(${TASTE_SUBLIB_SRC_ROOT}/requirements ${CMAKE_BINARY_DIR}/tastewidgets/requirements)
add_subdirectory(${TASTE_SUBLIB_SRC_ROOT}/reviews ${CMAKE_BINARY_DIR}/tastewidgets/reviews)

set(TasteQtWidgets_SRC
    ${CMAKE_CURRENT_BINARY_DIR}/${bindings_library}/gitlab_issue_wrapper.h
    ${CMAKE_CURRENT_BINARY_DIR}/${bindings_library}/gitlab_issue_wrapper.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/${bindings_library}/gitlab_label_wrapper.h
    ${CMAKE_CURRENT_BINARY_DIR}/${bindings_library}/gitlab_label_wrapper.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/${bindings_library}/gitlab_qgitlabclient_wrapper.h
    ${CMAKE_CURRENT_BINARY_DIR}/${bindings_library}/gitlab_qgitlabclient_wrapper.cpp

    ${CMAKE_CURRENT_BINARY_DIR}/${bindings_library}/requirement_requirement_wrapper.h
    ${CMAKE_CURRENT_BINARY_DIR}/${bindings_library}/requirement_requirement_wrapper.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/${bindings_library}/requirement_requirementsmanager_wrapper.h
    ${CMAKE_CURRENT_BINARY_DIR}/${bindings_library}/requirement_requirementsmanager_wrapper.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/${bindings_library}/requirement_requirementsmodelbase_wrapper.h
    ${CMAKE_CURRENT_BINARY_DIR}/${bindings_library}/requirement_requirementsmodelbase_wrapper.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/${bindings_library}/requirement_requirementswidget_wrapper.h
    ${CMAKE_CURRENT_BINARY_DIR}/${bindings_library}/requirement_requirementswidget_wrapper.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/${bindings_library}/requirement_wrapper.h
    ${CMAKE_CURRENT_BINARY_DIR}/${bindings_library}/requirement_wrapper.cpp

    ${CMAKE_CURRENT_BINARY_DIR}/${bindings_library}/reviews_review_wrapper.h
    ${CMAKE_CURRENT_BINARY_DIR}/${bindings_library}/reviews_review_wrapper.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/${bindings_library}/reviews_componentreviewsproxymodel_wrapper.h
    ${CMAKE_CURRENT_BINARY_DIR}/${bindings_library}/reviews_componentreviewsproxymodel_wrapper.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/${bindings_library}/reviews_reviewsmanager_wrapper.h
    ${CMAKE_CURRENT_BINARY_DIR}/${bindings_library}/reviews_reviewsmanager_wrapper.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/${bindings_library}/reviews_reviewsmodelbase_wrapper.h
    ${CMAKE_CURRENT_BINARY_DIR}/${bindings_library}/reviews_reviewsmodelbase_wrapper.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/${bindings_library}/reviews_componentreviewsproxymodel_wrapper.h
    ${CMAKE_CURRENT_BINARY_DIR}/${bindings_library}/reviews_componentreviewsproxymodel_wrapper.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/${bindings_library}/reviews_reviewswidget_wrapper.h
    ${CMAKE_CURRENT_BINARY_DIR}/${bindings_library}/reviews_reviewswidget_wrapper.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/${bindings_library}/reviews_wrapper.h
    ${CMAKE_CURRENT_BINARY_DIR}/${bindings_library}/reviews_wrapper.cpp

    ${CMAKE_CURRENT_BINARY_DIR}/${bindings_library}/tasteqtwidgets_module_wrapper.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/${bindings_library}/tasteqtwidgets_python.h

    ${CMAKE_CURRENT_BINARY_DIR}/${bindings_library}/tracecommon_issuesmanager_wrapper.h
    ${CMAKE_CURRENT_BINARY_DIR}/${bindings_library}/tracecommon_issuesmanager_wrapper.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/${bindings_library}/tracecommon_tracecommonmodelbase_wrapper.h
    ${CMAKE_CURRENT_BINARY_DIR}/${bindings_library}/tracecommon_tracecommonmodelbase_wrapper.cpp
)

# includes necessary to parse and build the classes specified on typesystem
set(TasteQtWidgets_include_paths
    $<JOIN:$<TARGET_PROPERTY:${original_library},INTERFACE_INCLUDE_DIRECTORIES>,${PATH_SEP}>
)

# A list of paths where shiboken should look for typesystem
set(TasteQtWidgets_typesystem_paths # PySide path, this variable was exposed by FindPySide6.cmake
    ${PYSIDE_TYPESYSTEMS}
)

# Libraries that will be necessary to link the target, this will used in the command 'target_link_libraries'
set(TasteQtWidgets_target_link_libraries ${original_library}
    ${MY_LINK_LIBS}
    ${Python3_LIBRARIES}
    QGitlabAPI
    requirements
    reviews
    tracecommon
)

set(LIBS_ICUI18N_DIR ${PYSIDE_BASEDIR}"/Qt/lib/")
set(CMAKE_LIBRARY_PATH ${CMAKE_LIBRARY_PATH} ${LIBS_ICUI18N_DIR})

add_library(${original_library} SHARED )

target_link_libraries(${original_library} PUBLIC
    QGitlabAPI
    requirements
    reviews
    tracecommon
)

set(QT_INCLUDES "")
foreach(QT_MODULE Qt6::Core Qt6::Widgets Qt6::Network Qt6::Gui QGitlabAPI requirements reviews tracecommon)
    get_property(QT_WIDGETS_INCLUDE_DIRS TARGET ${QT_MODULE} PROPERTY INTERFACE_INCLUDE_DIRECTORIES)
    foreach(INCLUDE_DIR ${QT_WIDGETS_INCLUDE_DIRS})
        if(NOT "${INCLUDE_DIR}" IN_LIST QT_INCLUDES)
            list(APPEND QT_INCLUDES "${INCLUDE_DIR}")
            message(STATUS "Adding INCLUDE path: " ${INCLUDE_DIR})
        endif()
    endforeach()
endforeach()

set(ALL_INCLUDE_PATHS
    ${QT_INCLUDES}
    ${SHIBOKEN_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/../tastewidgets
)
# Include flags/path that will be set in 'target_include_directories'
set(TasteQtWidgets_target_include_directories ${ALL_INCLUDE_PATHS})
set(TasteQtWidgets_include_paths ${ALL_INCLUDE_PATHS})

create_python_bindings(
    ${PROJECT_NAME}
    "${TasteQtWidgets_typesystem_paths}"
    "${TasteQtWidgets_include_paths}"
    "${TasteQtWidgets_SRC}"
    "${TasteQtWidgets_target_include_directories}"
    "${TasteQtWidgets_target_link_libraries}"
    ${CMAKE_CURRENT_SOURCE_DIR}/tasteqtwidgets_global.h
    ${CMAKE_CURRENT_SOURCE_DIR}/tasteqtwidgets_typesystem.xml
    ""
    ${CMAKE_CURRENT_BINARY_DIR}
)

message("\n")
message(STATUS "Resulting Python module to be installed: " ${TasteQtWidgets_PYTHON_BINDINGS_INSTALL_PREFIX})
message("\n")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/main.py.in ${CMAKE_CURRENT_SOURCE_DIR}/main.py)

install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/__init__.py" DESTINATION "${TasteQtWidgets_PYTHON_BINDINGS_INSTALL_PREFIX}")

