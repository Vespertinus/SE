set(SOURCES "main.cpp" "ImGuiWrapper.cpp")

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_executable(world_demo ${SOURCES})

if(TERRAIN)
        target_compile_definitions(world_demo PRIVATE SE_TERRAIN_ENABLED)
else()
        message(FATAL_ERROR "world_demo requires TERRAIN=ON")
endif()

# CLI parsing (scene_viewer pattern): --help / strict unknown-option handling.
find_package(Boost 1.54.0 REQUIRED COMPONENTS program_options)
include_directories(${Boost_INCLUDE_DIRS})

target_link_libraries(world_demo ${Boost_LIBRARIES} SECommon SE SEImGui)
