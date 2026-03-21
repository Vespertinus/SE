
set(SOURCES  "main.cpp"  "Scene.cpp"  "ImGuiWrapper.cpp")

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_executable(audio_demo ${SOURCES})

target_link_libraries(audio_demo SECommon SE SEImGui)
