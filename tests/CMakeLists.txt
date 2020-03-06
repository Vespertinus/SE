
enable_testing()

find_package(GTest REQUIRED)
message(STATUS "Found GTest library at ${GTEST_LIBRARIES}")

find_path(      GMOCK_INCLUDE_DIRS
                "gmock/gmock.h")
find_library(   GMOCK_LIBRARIES
                NAMES gmock
            )
if(GMOCK_INCLUDE_DIRS AND GMOCK_LIBRARIES)
        message(STATUS "Found GMock library at ${GMOCK_LIBRARIES}")
else()
        message(FATAL_ERROR "Failed to found GMock library")
endif()

include_directories (${GTEST_INCLUDE_DIRS} "." ${GMOCK_INCLUDE_DIRS} ${GMOCK_INCLUDE_DIRS})

set (SOURCES            "Main.cpp"
                        "SceneTreeTest.cpp")

add_executable(         scene_tree_test ${SOURCES})

target_link_libraries(  scene_tree_test
                        GTest::GTest
#                        GTest::Main
                        ${GMOCK_LIBRARIES}
                        SECommon
                        SE)

add_executable(         world_process_manager_test
                        "Main.cpp"
                        "WorldProcessManagerTest.cpp")
target_link_libraries(  world_process_manager_test
                        GTest::GTest
                        ${GMOCK_LIBRARIES}
                        SECommon
                        SE)

gtest_discover_tests(   scene_tree_test)
gtest_discover_tests(   world_process_manager_test)

#TODO add custom_command for resource processing
