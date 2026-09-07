
set(SOURCES "main.cpp" "ImGuiWrapper.cpp")

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_executable(moving_actors ${SOURCES})

target_link_libraries(moving_actors SECommon SE SEImGui)
