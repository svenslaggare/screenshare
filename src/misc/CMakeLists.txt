set(LOCAL_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/time_measurement.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/rate_sleeper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/network.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/concurrency.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/bit_rate_measurement.cpp
)

set(SOURCES ${SOURCES} ${LOCAL_SOURCES} PARENT_SCOPE)
