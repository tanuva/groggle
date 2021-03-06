cmake_minimum_required(VERSION 3.0)

project(groggle)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS true)

# nlohmann_json
set(JSON_BuildTests OFF CACHE INTERNAL "")
add_subdirectory(3rdparty/nlohmann_json)

add_executable(groggle
    src/color.cpp
    src/main.cpp
    src/olaoutput.cpp
    src/painput.cpp
    src/sdlinput.cpp
    src/spectrum.cpp
    src/timer.cpp
    src/mqttcontrol.cpp
)

target_include_directories(groggle SYSTEM PUBLIC 3rdparty/tclap-1.2.2/include)
target_compile_options(groggle PUBLIC -Wall -Wextra -pedantic -Werror)
set_property(TARGET groggle
    APPEND PROPERTY
    LINK_FLAGS "-pthread"
)

# Dependencies
# ============

find_package(PkgConfig REQUIRED)

# SDL2
find_package(SDL2 CONFIG REQUIRED)
target_include_directories(groggle PUBLIC ${SDL2_INCLUDE_DIRS})
target_link_libraries(groggle ${SDL2_LIBRARIES})

# PulseAudio
find_path(PULSEAUDIO_INCLUDE_DIR
    NAMES pulse/pulseaudio.h
    DOC "The PulseAudio include directory"
)

find_library(PULSEAUDIO_LIBRARY
    NAMES pulse
    DOC "The PulseAudio library"
)

target_include_directories(groggle PUBLIC ${PULSEAUDIO_INCLUDE_DIR})
target_link_libraries(groggle ${PULSEAUDIO_LIBRARY})

# OLA
# TODO Configurable installation path
#set(OLA_LIBDIR "/usr/lib/arm-linux-gnueabi")
target_link_libraries(groggle "ola")
target_link_libraries(groggle "olacommon")
#target_include_directories(groggle PUBLIC ${OLA_INCLUDE_DIRS})
#target_compile_options(groggle PUBLIC ${OLA_CFLAGS_OTHER})

target_compile_options(groggle PUBLIC -D_THREAD_SAFE)

# fftw3
pkg_check_modules(FFTW REQUIRED fftw3f)
target_link_libraries(groggle ${FFTW_LIBRARIES})
target_include_directories(groggle PUBLIC ${FFTW_INCLUDE_DIRS})
target_compile_options(groggle PUBLIC ${FFTW_CFLAGS_OTHER})

# libmosquitto
find_path(MOSQUITTO_INCLUDE_DIR
    NAMES mosquitto.h
    DOC "The MQTT client library's include directory"
)
find_library(MOSQUITTO_LIBRARY
    NAMES mosquitto
    DOC "The MQTT client library"
)

target_include_directories(groggle PUBLIC ${MOSQUITTO_INCLUDE_DIR})
target_link_libraries(groggle ${MOSQUITTO_LIBRARY})

# nlohmann_json
target_include_directories(groggle PUBLIC 3rdparty/nlohmann_json/include)
target_link_libraries(groggle nlohmann_json::nlohmann_json)

# Unit Tests
add_executable(tests
    3rdparty/catch2/catch_amalgamated.cpp
    src/tests.cpp
    src/color.cpp
)

target_include_directories(tests SYSTEM PUBLIC 3rdparty)
