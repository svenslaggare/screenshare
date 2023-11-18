cmake_minimum_required(VERSION 3.26)
project(screenshare)

set(CMAKE_CXX_STANDARD 20)

##############################################################################################################
# Targets
##############################################################################################################

add_subdirectory(src)

add_executable(screenshare ${SOURCES} src/main.cpp)

##############################################################################################################
# Dependencies
##############################################################################################################

find_package(PkgConfig REQUIRED)
find_package(ZLIB REQUIRED)
find_package(PNG REQUIRED)
find_package(Boost REQUIRED COMPONENTS system)
find_package(X11 REQUIRED)

pkg_check_modules(LIBAV REQUIRED IMPORTED_TARGET
        libavcodec
        libavformat
        libavfilter
        libavdevice
        libswresample
        libswscale
        libavutil
)

pkg_check_modules(GTKMM gtkmm-3.0)

##############################################################################################################
# Linking
##############################################################################################################

target_link_libraries(screenshare PRIVATE ${ZLIB_LIBRARIES})
target_link_libraries(screenshare PRIVATE ${PNG_LIBRARIES})
target_link_libraries(screenshare PRIVATE PkgConfig::LIBAV)
target_link_libraries(screenshare PRIVATE ${Boost_LIBRARIES})
target_link_libraries(screenshare PRIVATE ${GTKMM_LIBRARIES})
target_link_libraries(screenshare PRIVATE ${X11_LIBRARIES})

##############################################################################################################
# Include dirs
##############################################################################################################

target_include_directories(screenshare PRIVATE ${GTKMM_INCLUDE_DIRS})