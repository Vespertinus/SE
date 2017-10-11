
set(SOURCES             "main.cpp")

add_executable(         simple_engine
                        ${SOURCES})

target_link_libraries(  simple_engine
                        SECommon)

