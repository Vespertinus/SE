
if(GPU)
        set(SOURCES     "main_debug.cpp"
                        "${CMAKE_SOURCE_DIR}/core/InputManager.cpp"
                        )
else()
        set(SOURCES     "main.cpp")

endif()

set(SOURCES             ${SOURCES}
                        ${COMMON_SOURCES})

add_executable(         offscreen_rendering
                        ${SOURCES})

target_link_libraries(offscreen_rendering)
