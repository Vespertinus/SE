
set(SOURCES  "main.cpp"  "Scene.cpp"  "ImGuiWrapper.cpp")

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_executable(sound_demo ${SOURCES})

target_link_libraries(sound_demo SECommon SE SEImGui)
