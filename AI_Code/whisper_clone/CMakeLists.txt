cmake_minimum_required(VERSION 3.15)
project(WhisperClone VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Options
option(BUILD_TESTS "Build test programs" ON)

# Find dependencies
find_package(Threads REQUIRED)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# Source files
set(SOURCES
    src/audio_processor.cpp
    src/whisper_model.cpp
    src/utils.cpp
)

# Header files
set(HEADERS
    include/audio_processor.h
    include/whisper_model.h
    include/utils.h
)

# Create library
add_library(whisper_lib STATIC ${SOURCES} ${HEADERS})
target_include_directories(whisper_lib PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(whisper_lib Threads::Threads)

# Main executable
add_executable(whisper_clone src/main.cpp)
target_link_libraries(whisper_clone whisper_lib)

# Installation
install(TARGETS whisper_clone DESTINATION bin)
install(FILES ${HEADERS} DESTINATION include/whisper_clone)
