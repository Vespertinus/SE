
set(SOURCES  "main.cpp"  "Scene.cpp")

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_executable(ui_demo ${SOURCES})

target_link_libraries(ui_demo PRIVATE SECommon SE)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_link_libraries(ui_demo PRIVATE rmlui_debugger)
        target_compile_definitions(ui_demo PRIVATE SE_UI_DEBUGGER)
endif()
