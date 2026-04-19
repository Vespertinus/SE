
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

add_custom_target(all_tests)
add_dependencies(all_tests 

                        scene_tree_test
                        world_process_manager_test
                        allocator_test
                        clock_test
                        entity_manager_test 
                        entity_template_test
                        entity_template_utility_test
                )

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

add_executable(         allocator_test
                        "Main.cpp"
                        "AllocatorTest.cpp")
target_link_libraries(  allocator_test
                        GTest::GTest
                        ${GMOCK_LIBRARIES}
                        SECommon
                        SE)
gtest_discover_tests(   allocator_test)

add_executable(         clock_test
                        "Main.cpp"
                        "ClockTest.cpp")
target_link_libraries(  clock_test
                        GTest::GTest
                        ${GMOCK_LIBRARIES}
                        SECommon
                        SE)
gtest_discover_tests(   clock_test)

add_executable(         entity_manager_test
                        "Main.cpp"
                        "EntityManagerTest.cpp")
target_link_libraries(  entity_manager_test
                        GTest::GTest
                        ${GMOCK_LIBRARIES}
                        SECommon
                        SE)
gtest_discover_tests(   entity_manager_test)

add_executable(         entity_template_utility_test
                        "Main.cpp"
                        "EntityTemplateUtilityTest.cpp")
target_link_libraries(  entity_template_utility_test
                        GTest::GTest
                        ${GMOCK_LIBRARIES}
                        SECommon
                        SE)
gtest_discover_tests(   entity_template_utility_test)

add_executable(         entity_template_test
                        "Main.cpp"
                        "EntityTemplateTest.cpp")
target_link_libraries(  entity_template_test
                        GTest::GTest
                        ${GMOCK_LIBRARIES}
                        SECommon
                        SE)
gtest_discover_tests(   entity_template_test)

#TODO add custom_command for resource processing
