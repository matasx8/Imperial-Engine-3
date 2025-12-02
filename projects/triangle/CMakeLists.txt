cmake_minimum_required(VERSION 3.20)
project(Triangle LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Path to the engine (relative to this project)
set(ENGINE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../engine")

# Bring engine into this project
add_subdirectory(${ENGINE_ROOT} ${CMAKE_BINARY_DIR}/engine_build)

# Triangle sources
file(GLOB_RECURSE TRIANGLE_SOURCES CONFIGURE_DEPENDS
    "src/*.cpp"
    "src/*.h"
)

add_executable(triangle ${TRIANGLE_SOURCES})

target_include_directories(triangle PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(triangle PRIVATE ImperialEngine3_Engine)