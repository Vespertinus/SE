
if(GPU)
        set(SOURCES     "main_debug.cpp")
else()
        set(SOURCES     "main.cpp"
                        "Scene.cpp"
                        )

endif()

add_executable(         offscreen_rendering
                        ${SOURCES})
target_link_libraries(  offscreen_rendering
                        SECommon)
