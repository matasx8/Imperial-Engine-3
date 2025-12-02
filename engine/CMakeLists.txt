project(ImperialEngine3_Engine LANGUAGES CXX)

file(GLOB_RECURSE ENGINE_SOURCES CONFIGURE_DEPENDS
    "src/Dummy.cpp"
)

add_library(ImperialEngine3_Engine STATIC ${ENGINE_SOURCES})

target_include_directories(ImperialEngine3_Engine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)