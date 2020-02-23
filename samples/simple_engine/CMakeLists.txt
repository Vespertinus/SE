
set(SOURCES             "main.cpp"
                        "BlinkProcess.cpp")

include_directories(    ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(         simple_engine
                        ${SOURCES})

target_link_libraries(  simple_engine
                        SECommon
                        SE)

