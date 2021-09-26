
set(TOOLS_INC_DIRS)
set(TOOLS_LIBRARIES_LIST)

### check dependencies
find_package(Boost 1.54.0 REQUIRED COMPONENTS program_options filesystem)
include_directories (${Boost_INCLUDE_DIRS})
list(APPEND TOOLS_LIBRARIES_LIST ${Boost_LIBRARIES})

###

include_directories(${TOOLS_INC_DIRS} ${CMAKE_CURRENT_SOURCE_DIR})

set(SOURCES     "Main.cpp"
                "Scene.cpp"
                "ImGuiWrapper.cpp"
                )

add_executable(         scene_viewer
                        ${SOURCES})

target_link_libraries(  scene_viewer
                        ${TOOLS_LIBRARIES_LIST}
                        SECommon
                        SE
                        SEImGui)
