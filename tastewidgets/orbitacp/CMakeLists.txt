# The ACP client that talks to `orbit acp`. A copy of the library used by Space
# Creator, so this widget stays independent of it. Namespaces, targets and
# headers are prefixed with `tqtw_` to avoid clashing with Space Creator's own
# copy when both are built into the same binary.
set(TQTW_ORBITACP_TARGET tqtw_orbitacp)

add_library(${TQTW_ORBITACP_TARGET} STATIC)

target_sources(${TQTW_ORBITACP_TARGET}
    PUBLIC
        tqtw_types.h
        tqtw_wire.h
        tqtw_conversation.h
    PRIVATE
        types.cpp
        wire.cpp
        conversation.cpp
)

target_include_directories(${TQTW_ORBITACP_TARGET} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(${TQTW_ORBITACP_TARGET} PUBLIC Qt6::Core)
