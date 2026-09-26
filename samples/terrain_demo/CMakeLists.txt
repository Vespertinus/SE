
set(SOURCES "main.cpp" "ImGuiWrapper.cpp")

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_executable(terrain_demo ${SOURCES})

if(TERRAIN)
        target_compile_definitions(terrain_demo PRIVATE SE_TERRAIN_ENABLED)
else()
        message(FATAL_ERROR "terrain_demo requires TERRAIN=ON")
endif()

target_link_libraries(terrain_demo SECommon SE SEImGui)
