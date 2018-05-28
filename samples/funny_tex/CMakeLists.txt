
set(SOURCES             "FunnyTex.cpp")

add_executable(         funny_tex
                        ${SOURCES})

target_link_libraries(  funny_tex
                        SECommon
                        SE)

