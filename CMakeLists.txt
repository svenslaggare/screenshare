cmake_minimum_required(VERSION 3.26)
project(screenshare)

set(CMAKE_CXX_STANDARD 20)

##############################################################################################################
# Options
##############################################################################################################

##############################################################################################################
# Targets
##############################################################################################################

add_subdirectory(src)

add_executable(screenshare ${SOURCES} src/main.cpp
        src/misc/bit_rate_measurement.cpp
        src/misc/bit_rate_measurement.h
)

##############################################################################################################
# Dependencies
##############################################################################################################

include(FetchContent)

FetchContent_Declare(
    fmt
    URL      https://github.com/fmtlib/fmt/releases/download/10.1.1/fmt-10.1.1.zip
    URL_HASH SHA256=b84e58a310c9b50196cda48d5678d5fa0849bca19e5fdba6b684f0ee93ed9d1b
)

FetchContent_MakeAvailable(fmt)

add_dependencies(screenshare fmt)

find_package(PkgConfig REQUIRED)
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

target_link_libraries(screenshare PRIVATE fmt)
target_link_libraries(screenshare PRIVATE PkgConfig::LIBAV)
target_link_libraries(screenshare PRIVATE ${Boost_LIBRARIES})
target_link_libraries(screenshare PRIVATE ${GTKMM_LIBRARIES})
target_link_libraries(screenshare PRIVATE ${X11_LIBRARIES} Xtst)

##############################################################################################################
# Include dirs
##############################################################################################################

target_include_directories(screenshare PRIVATE ${GTKMM_INCLUDE_DIRS})