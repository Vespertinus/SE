
if(GPU)
        set(SOURCES     "main_debug.cpp")
else()
        set(SOURCES     "main.cpp")
endif()
list(APPEND             SOURCES "Scene.cpp")

include_directories(    ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(         offscreen_rendering
                        ${SOURCES})
target_link_libraries(  offscreen_rendering
                        SE
                        SECommon)
