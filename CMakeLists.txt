cmake_minimum_required (VERSION 3.0)

project (groggel)
set (CMAKE_CXX_STANDARD 14)

add_executable (groggel
    src/light.cpp
    src/main.cpp
    src/spectrum.cpp
    src/timer.cpp
)

# Dependencies
# ============

find_package(PkgConfig REQUIRED)

# SDL2
find_package(SDL2 CONFIG REQUIRED)
target_include_directories(groggel PUBLIC ${SDL2_INCLUDE_DIRS})
target_link_libraries(groggel ${SDL2_LIBRARIES})

# OLA
if (APPLE)
    # This is horribly fragile but works for me for now. Apparently cmake isn't
    # able to find libola without further hints which I'm not willing to figure
    # out right now.
    target_link_libraries(groggel -L../../../../usr/local/Cellar/protobuf@3.1/3.1.0/lib -L../../../../usr/local/Cellar/ola/0.10.7/lib -lola -lolacommon -lprotobuf)
    target_include_directories(groggel PUBLIC ../../../../usr/local/Cellar/protobuf@3.1/3.1.0/include ../../../../usr/local/Cellar/ola/0.10.7/include)
    target_compile_options(groggel PUBLIC -D_THREAD_SAFE)
else (APPLE)
    pkg_check_modules(OLA REQUIRED ola)
    target_link_libraries(groggel ${OLA_LIBRARIES})
    target_include_directories(groggel PUBLIC ${OLA_INCLUDE_DIRS})
    target_compile_options(groggel PUBLIC ${OLA_CFLAGS_OTHER})
endif (APPLE)

# fftw3
if (APPLE)
    target_link_libraries(groggel -L/usr/local/Cellar/fftw/3.3.8/lib -lfftw3f)
    target_include_directories(groggel PUBLIC ../../../../usr/local/Cellar/fftw/3.3.8/include)
else (APPLE)
    # TODO
endif (APPLE)
