
set(SOURCES "main.cpp")

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_executable(pbr_demo ${SOURCES})

target_link_libraries(pbr_demo SECommon SE)
