
set(SOURCES             "main.cpp"
                        "${CMAKE_SOURCE_DIR}/core/InputManager.cpp"
                        )

add_executable(         simple_engine
                        ${SOURCES})

target_link_libraries(simple_engine)

