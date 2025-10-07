set(LIB_NAME requirements)

remove_definitions(-DQT_NO_KEYWORDS)

add_library(${LIB_NAME} STATIC)

target_sources(${LIB_NAME} PRIVATE
  addnewrequirementdialog.cpp
  addnewrequirementdialog.h
  basicrequirementdialog.ui
  checkedfilterproxymodel.cpp
  checkedfilterproxymodel.h
  selectsourcedialog.cpp
  selectsourcedialog.h
  selectsourcedialog.ui
  editrequirementdialog.cpp
  editrequirementdialog.h
  gitlab/gitlabrequirements.cpp
  gitlab/gitlabrequirements.h
  excel.cpp
  excel.h
  reqif.cpp
  reqif.h
  requirement.cpp
  requirement.h
  requirementsmanager.cpp
  requirementsmanager.h
  requirementsmodelcommon.cpp
  requirementsmodelcommon.h
  requirementsmodelbase.cpp
  requirementsmodelbase.h
  requirementswidget.cpp
  requirementswidget.h
  requirementswidget.ui
  virtualassistantdialog.cpp
  virtualassistantdialog.h
  virtualassistantdialog.ui
  ../tracecommon/tracecommonresources.qrc
)

target_include_directories(${LIB_NAME} ${QXLSX_PUBLIC_HEADERS} PUBLIC .)
target_link_libraries(${LIB_NAME}
    PUBLIC Qt6::Core Qt6::Widgets tracecommon QXlsx
    PRIVATE Qt6::Concurrent Qt6::Xml QGitlabAPI)
target_compile_definitions(${LIB_NAME} PUBLIC QGITLABAPI_LIBRARY QT_DEBUG_OUTPUT)
