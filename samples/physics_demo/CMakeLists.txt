
set(SOURCES "main.cpp")

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_executable(physics_demo ${SOURCES})

target_link_libraries(physics_demo SECommon SE)
