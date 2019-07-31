
set(SOURCES             "FunnyTex.cpp")

include_directories(    ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(         funny_tex
                        ${SOURCES})

target_link_libraries(  funny_tex
                        SECommon
                        SE)

