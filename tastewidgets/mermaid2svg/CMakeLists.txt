# A C++ port of mermaid2svg, used to render ```mermaid diagrams in the Orbit
# chat. A copy of the library used by Space Creator, so this widget stays
# independent of it. Namespaces, targets and headers are prefixed with `tqtw_`
# to avoid clashing with Space Creator's own copy.
#
# License: MIT - a port of mermaid2svg, which is MIT licensed like the mermaid
# it reimplements. See LICENSE in this directory.
set(TQTW_M2S_TARGET tqtw_mermaid2svg)

add_library(${TQTW_M2S_TARGET} STATIC)

target_sources(${TQTW_M2S_TARGET} PRIVATE
    tqtw_m2sapi.h
    tqtw_m2sbase.h
    tqtw_m2scolors.h
    tqtw_m2scurves.h
    tqtw_m2sdagre.h
    tqtw_m2sdagrecommon.h
    tqtw_m2serrors.h
    tqtw_m2sflowchart.h
    tqtw_m2ser.h
    tqtw_m2sfrontmatter.h
    tqtw_m2ssequence.h
    tqtw_m2sclass.h
    tqtw_m2sstate.h
    tqtw_m2sintersect.h
    tqtw_m2sjson.h
    tqtw_m2ssvg.h
    tqtw_m2stext.h
    tqtw_m2stheme.h
    m2sapi.cpp
    m2sbase.cpp
    m2scolors.cpp
    m2scurves.cpp
    m2serrors.cpp
    m2sflowchart.cpp
    m2ser.cpp
    m2sfrontmatter.cpp
    m2ssequence.cpp
    m2sclass.cpp
    m2sstate.cpp
    m2sintersect.cpp
    m2sjson.cpp
    m2sdagre.cpp
    m2sdagrecommon.cpp
    m2ssvg.cpp
    m2stext.cpp
    m2stheme.cpp
)

target_include_directories(${TQTW_M2S_TARGET} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(${TQTW_M2S_TARGET} PUBLIC Qt6::Core)

set_target_properties(${TQTW_M2S_TARGET} PROPERTIES AUTOMOC OFF)
