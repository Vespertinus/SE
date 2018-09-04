
set(TOOLS_INC_DIRS)
set(TOOLS_LIBRARIES_LIST)

### check dependencies
find_package(Boost 1.54.0 REQUIRED COMPONENTS program_options filesystem)
include_directories (${Boost_INCLUDE_DIRS})
list(APPEND TOOLS_LIBRARIES_LIST ${Boost_LIBRARIES})

###

include_directories(${TOOLS_INC_DIRS})

set(SOURCES     "Main.cpp"
                "Scene.cpp"
                "${CMAKE_SOURCE_DIR}/common/ImGui.cpp"
                "${CMAKE_SOURCE_DIR}/third_party/imgui/imgui.cpp"
                "${CMAKE_SOURCE_DIR}/third_party/imgui/imgui_draw.cpp"
                )

add_executable(         scene_viewer
                        ${SOURCES})

target_link_libraries(  scene_viewer
                        ${TOOLS_LIBRARIES_LIST}
                        SECommon
                        SE)
