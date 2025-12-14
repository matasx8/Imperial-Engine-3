cmake_minimum_required(VERSION 3.20)
project(Demo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Path to the engine (relative to this project)
set(ENGINE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../engine")

# Bring engine into this project
add_subdirectory(${ENGINE_ROOT} ${CMAKE_BINARY_DIR}/engine_build)

# Demo sources
file(GLOB_RECURSE DEMO_SOURCES CONFIGURE_DEPENDS
    "src/*.cpp"
    "src/*.h"
)

add_executable(demo ${DEMO_SOURCES})

target_include_directories(demo PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(demo PRIVATE ImperialEngine3_Engine)

set(SHADER_SPV_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/spv)
file(MAKE_DIRECTORY ${SHADER_SPV_DIR})

function(add_shader TARGET SHADER OUTPUT)
    set(OUTPUT_FILE ${SHADER_SPV_DIR}/${OUTPUT}.h)

    get_filename_component(SHADER_ABS ${SHADER} ABSOLUTE)

    add_custom_command(
        OUTPUT ${OUTPUT_FILE}
        COMMAND ${Vulkan_GLSLANG_VALIDATOR_EXECUTABLE}
            --target-env spirv1.3
            -V
            --vn ${OUTPUT}
            -o ${OUTPUT_FILE}
            ${SHADER_ABS}
        DEPENDS ${SHADER_ABS}
        COMMENT "Compiling shader ${SHADER}"
        VERBATIM
    )

    add_custom_target(shader_${OUTPUT} DEPENDS ${OUTPUT_FILE})
    add_dependencies(${TARGET} shader_${OUTPUT})
endfunction()


add_shader(demo src/shaders/triangle.vert triangle_vert)
add_shader(demo src/shaders/triangle.frag triangle_frag)