cmake_minimum_required (VERSION 3.0)

project (groggle)
set (CMAKE_CXX_STANDARD 14)

add_executable (groggle
    src/light.cpp
    src/main.cpp
    src/spectrum.cpp
    src/timer.cpp
)

target_include_directories (groggle PUBLIC 3rdparty/tclap-1.2.2/include)
if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
#    target_compile_options(groggle PUBLIC "-pthread")
    set_property(TARGET groggle
	APPEND PROPERTY
	LINK_FLAGS "-pthread"
    )
endif()

# Dependencies
# ============

find_package(PkgConfig REQUIRED)

# SDL2
find_package(SDL2 CONFIG REQUIRED)
target_include_directories(groggle PUBLIC ${SDL2_INCLUDE_DIRS})
target_link_libraries(groggle ${SDL2_LIBRARIES})

# OLA
if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    # This is horribly fragile but works for me for now. Apparently cmake isn't
    # able to find libola without further hints which I'm not willing to figure
    # out right now.
    target_link_libraries(groggle -L../../../../usr/local/Cellar/protobuf@3.1/3.1.0/lib -L../../../../usr/local/Cellar/ola/0.10.7/lib -lola -lolacommon -lprotobuf)
    target_include_directories(groggle PUBLIC ../../../../usr/local/Cellar/protobuf@3.1/3.1.0/include ../../../../usr/local/Cellar/ola/0.10.7/include)
else()
    # TODO Configurable installation path
    #set(OLA_LIBDIR "/usr/lib/arm-linux-gnueabi")
    target_link_libraries(groggle "ola")
    target_link_libraries(groggle "olacommon")
    #target_include_directories(groggle PUBLIC ${OLA_INCLUDE_DIRS})
    #target_compile_options(groggle PUBLIC ${OLA_CFLAGS_OTHER})
endif()

target_compile_options(groggle PUBLIC -D_THREAD_SAFE)

# fftw3
if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    target_link_libraries(groggle -L/usr/local/Cellar/fftw/3.3.8/lib -lfftw3f)
    target_include_directories(groggle PUBLIC ../../../../usr/local/Cellar/fftw/3.3.8/include)
else()
    pkg_check_modules(FFTW REQUIRED fftw3f)
    target_link_libraries(groggle ${FFTW_LIBRARIES})
    target_include_directories(groggle PUBLIC ${FFTW_INCLUDE_DIRS})
    target_compile_options(groggle PUBLIC ${FFTW_CFLAGS_OTHER})
endif()
