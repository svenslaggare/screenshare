set(LOCAL_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/video_player.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/info_text_buffer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/actions.cpp
)

set(SOURCES ${SOURCES} ${LOCAL_SOURCES} PARENT_SCOPE)
