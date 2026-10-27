cmake_minimum_required(VERSION 3.22.1)
project("mididawnative")

set(CMAKE_CXX_STANDARD 17)

include(FetchContent)
FetchContent_Declare(
    oboe
    GIT_REPOSITORY https://github.com
    GIT_TAG tags/v1.8.0
)
FetchContent_MakeAvailable(oboe)

add_library(mididawnative SHARED
    midi_daw_engine.cpp
)

target_link_libraries(mididawnative
    oboe
    android
    amidi
    log
)
