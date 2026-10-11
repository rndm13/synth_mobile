cmake_minimum_required(VERSION 3.10)

project(synth)

enable_language(C)
set(CMAKE_C_STANDARD 11)

set(CMAKE_CXX_FLAGS_DEBUG_INIT "-Wall -Wextra -Wshadow -Wnon-virtual-dtor -pedantic -Wcast-align -Wconversion -Wuseless-cast -Wnull-dereference -Wlogical-op -fopenmp")
set(CMAKE_CXX_FLAGS_RELEASE_INIT "-Wall -fopenmp")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# External libs

find_package(Threads REQUIRED)

find_package(OpenMP)

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
)
target_compile_definitions(${PROJECT_NAME} PUBLIC
    ASSET="${ASSET_PATH}"
)

target_link_libraries(${PROJECT_NAME} PUBLIC
    m
    raylib
    ${CMAKE_THREAD_LIBS_INIT}
    "$<$<CONFIG:DEBUG>:-fsanitize=address,undefined,leak>"
)

if(OpenMP_CXX_FOUND)
    target_link_libraries(${PROJECT_NAME} PUBLIC
        OpenMP::OpenMP_CXX
    )
endif()
