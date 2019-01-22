cmake_minimum_required (VERSION 3.0)

project (groggel)
set (CMAKE_CXX_STANDARD 14)

add_executable (groggel
    src/audiodecoder.cpp
    src/main.cpp
    src/spectrum.cpp
    src/timer.cpp
)

# Dependencies
# ============

find_package(PkgConfig REQUIRED)

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

# Audio file decoding
# TODO Find libs directly via cmake! (also above...)
pkg_check_modules(LIBAVFORMAT REQUIRED libavformat)
target_link_libraries(groggel -L${LIBAVFORMAT_LIBRARY_DIRS} ${LIBAVFORMAT_LIBRARIES})
target_include_directories(groggel PUBLIC ${LIBAVFORMAT_INCLUDE_DIRS})
target_compile_options(groggel PUBLIC ${LIBAVFORMAT_CFLAGS_OTHER})

pkg_check_modules(LIBAVFILTER REQUIRED libavfilter)
target_link_libraries(groggel -L${LIBAVFILTER_LIBRARY_DIRS} ${LIBAVFILTER_LIBRARIES})
target_include_directories(groggel PUBLIC ${LIBAVFILTER_INCLUDE_DIRS})
target_compile_options(groggel PUBLIC ${LIBAVFILTER_CFLAGS_OTHER})

pkg_check_modules(LIBAVCODEC REQUIRED libavcodec)
target_link_libraries(groggel -L${LIBAVCODEC_LIBRARY_DIRS} ${LIBAVCODEC_LIBRARIES})
target_include_directories(groggel PUBLIC ${LIBAVCODEC_INCLUDE_DIRS})
target_compile_options(groggel PUBLIC ${LIBAVCODEC_CFLAGS_OTHER})

pkg_check_modules(LIBAVRESAMPLE REQUIRED libavresample)
target_link_libraries(groggel -L${LIBAVRESAMPLE_LIBRARY_DIRS} ${LIBAVRESAMPLE_LIBRARIES})
target_include_directories(groggel PUBLIC ${LIBAVRESAMPLE_INCLUDE_DIRS})
target_compile_options(groggel PUBLIC ${LIBAVRESAMPLE_CFLAGS_OTHER})

pkg_check_modules(LIBSWRESAMPLE REQUIRED libswresample)
target_link_libraries(groggel -L${LIBSWRESAMPLE_LIBRARY_DIRS} ${LIBSWRESAMPLE_LIBRARIES})
target_include_directories(groggel PUBLIC ${LIBSWRESAMPLE_INCLUDE_DIRS})
target_compile_options(groggel PUBLIC ${LIBSWRESAMPLE_CFLAGS_OTHER})

# Audio playback
# TODO find_library? Doesn't work with target_link_libraries atm...
target_link_libraries(groggel -L/usr/local/lib -lao)
target_include_directories(groggel PUBLIC /usr/local/include)

# fftw3
if (APPLE)
    target_link_libraries(groggel -L/usr/local/Cellar/fftw/3.3.8/lib -lfftw3f)
    target_include_directories(groggel PUBLIC ../../../../usr/local/Cellar/fftw/3.3.8/include)
else (APPLE)
    # TODO
endif (APPLE)
