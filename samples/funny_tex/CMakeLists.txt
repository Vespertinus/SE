
set(SOURCES             "FunnyTex.cpp"
                        "${CMAKE_SOURCE_DIR}/core/InputManager.cpp"
                        )

add_executable(         funny_tex
                        ${SOURCES})

target_link_libraries(funny_tex)

