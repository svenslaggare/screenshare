set(LOCAL_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/common.h
    ${CMAKE_CURRENT_SOURCE_DIR}/x11.cpp
)

set(SOURCES ${SOURCES} ${LOCAL_SOURCES} PARENT_SCOPE)
