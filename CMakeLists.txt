cmake_minimum_required(VERSION 3.12)

project(synth)

enable_language(C)
set(CMAKE_C_STANDARD 11)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/CMake")
set(CMAKE_CXX_FLAGS_DEBUG_INIT "-Wall -Wextra -Wshadow -Wnon-virtual-dtor -pedantic -Wcast-align -Wconversion -Wuseless-cast -Wnull-dereference -Wlogical-op -fopenmp")
set(CMAKE_CXX_FLAGS_RELEASE_INIT "-Wall -fopenmp")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# External libs

find_package(Threads REQUIRED)

find_package(OpenMP)

find_package(jack)
if (NOT jack_FOUND)
    message(FATAL_ERROR "JACK Library not found.")
endif()

add_subdirectory(external/raylib)

# Assets

set(ASSET_PATH "../assets/")

# Executable

add_executable(${PROJECT_NAME}
    src/main.c
    src/env.c
    src/fft.c
    src/file.c
    src/filter.c
    src/gui_elements.c
    src/ini.c
    src/osc.c
    src/synth.c
    src/voice.c
    src/log.c
    src/ring_buffer.c
    src/jack_midi_interface.c
)
target_compile_definitions(${PROJECT_NAME} PUBLIC
    ASSET="${ASSET_PATH}"
)

include_directories(${PROJECT_NAME} ${JACK_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} PUBLIC
    m
    raylib
    ${CMAKE_THREAD_LIBS_INIT}
    ${JACK_LIBRARIES}
    "$<$<CONFIG:DEBUG>:-fsanitize=address,undefined,leak>"
)
if (${JACK_DEFINITIONS})
    add_definitions(${PROJECT_NAME} ${JACK_DEFINITIONS})
endif()

if(OpenMP_CXX_FOUND)
    target_link_libraries(${PROJECT_NAME} PUBLIC OpenMP::OpenMP_CXX)
endif()
