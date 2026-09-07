
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
                        state_machine_test
                        character_controller_test
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

add_executable(         state_machine_test
                        "Main.cpp"
                        "StateMachineTest.cpp")
target_link_libraries(  state_machine_test
                        GTest::GTest
                        ${GMOCK_LIBRARIES}
                        SECommon
                        SE)
gtest_discover_tests(   state_machine_test)

add_executable(         character_controller_test
                        "Main.cpp"
                        "CharacterControllerTest.cpp")
target_link_libraries(  character_controller_test
                        GTest::GTest
                        ${GMOCK_LIBRARIES}
                        SECommon
                        SE)
gtest_discover_tests(   character_controller_test)

# ---------------------------------------------------------------------------
# Functional / integration tests + harness (tests/functional).
#
# House rule: exactly ONE test .cpp per executable. AnimGraphRuntime.tcc defines
# AnimGraphInstance::kEmptyName (external-linkage class static) in a header and
# each test TU compiles the engine's SE_IMPL blocks — two TUs in one binary
# would break ODR/link. TestMain.cpp is the only other TU and includes no
# engine impls.
#
# All targets run with CWD = repo root so Config::sResourceDir ("resource/")
# resolves real assets.
# ---------------------------------------------------------------------------
set(FUNCTIONAL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/functional")
include_directories("${FUNCTIONAL_DIR}")

add_custom_target(all_functional_tests)

function(se_add_functional_test NAME SOURCE)
        add_executable(${NAME}
                        "${FUNCTIONAL_DIR}/TestMain.cpp"
                        "${FUNCTIONAL_DIR}/${SOURCE}")
        target_link_libraries(${NAME}
                        GTest::GTest
                        ${GMOCK_LIBRARIES}
                        SECommon
                        SE)
        gtest_discover_tests(${NAME}
                        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
                        DISCOVERY_TIMEOUT 120
                        TEST_PREFIX "functional_")
        add_dependencies(all_functional_tests ${NAME})
endfunction()

se_add_functional_test(anim_evaluator_test AnimEvaluatorTest.cpp)
se_add_functional_test(anim_clip_test AnimClipTest.cpp)
se_add_functional_test(anim_graph_instance_test AnimGraphInstanceTest.cpp)
se_add_functional_test(anim_blend_test AnimBlendTest.cpp)
se_add_functional_test(anim_events_test AnimEventsTest.cpp)
se_add_functional_test(anim_determinism_test AnimDeterminismTest.cpp)
se_add_functional_test(anim_assets_test AnimAssetsTest.cpp)

# Animator tests need a HEADLESS AnimatedModel: the shadow/ dir shadows
# units/AnimatedModel.h for this ONE target (include-path priority, single TU,
# never add shadow/ to global include dirs). See shadow/AnimatedModel.h.
add_executable(anim_animator_test
                "${FUNCTIONAL_DIR}/TestMain.cpp"
                "${FUNCTIONAL_DIR}/AnimAnimatorTest.cpp")
target_include_directories(anim_animator_test BEFORE PRIVATE
                "${FUNCTIONAL_DIR}/shadow")
target_link_libraries(anim_animator_test
                GTest::GTest
                ${GMOCK_LIBRARIES}
                SECommon
                SE)
gtest_discover_tests(anim_animator_test
                WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
                DISCOVERY_TIMEOUT 120
                TEST_PREFIX "functional_")
add_dependencies(all_functional_tests anim_animator_test)

# ---------------------------------------------------------------------------
# Physics functional suites (see agent_workspace/artifacts/func_tests_physics_001.md).
# Same house rules as the animation tier. PhysicsHarness.h owns a per-test
# PhysicsSystem: Jolt's Factory is a process global, so exactly ONE
# PhysicsSystem is initialized at any time (per-test instance in PhysTestBase
# suites, engine singleton in EnginePhysTestBase suites).
# ---------------------------------------------------------------------------
se_add_functional_test(phys_body_test PhysicsBodyTest.cpp)
se_add_functional_test(phys_query_test PhysicsQueryTest.cpp)
se_add_functional_test(phys_event_test PhysicsEventTest.cpp)
se_add_functional_test(phys_character_test PhysicsCharacterTest.cpp)
se_add_functional_test(phys_scene_test PhysicsSceneTest.cpp)
se_add_functional_test(phys_determinism_test PhysicsDeterminismTest.cpp)

# TriggerVolume suite: the application-level component enters the test
# composition via a target-local shadow App.h (precedent: anim_animator_test's
# shadow/AnimatedModel — never add trigger_app/ to global include dirs).
add_executable(phys_trigger_volume_test
                "${FUNCTIONAL_DIR}/TestMain.cpp"
                "${FUNCTIONAL_DIR}/PhysicsTriggerVolumeTest.cpp")
target_include_directories(phys_trigger_volume_test BEFORE PRIVATE
                "${FUNCTIONAL_DIR}/trigger_app")
target_link_libraries(phys_trigger_volume_test
                GTest::GTest
                ${GMOCK_LIBRARIES}
                SECommon
                SE)
gtest_discover_tests(phys_trigger_volume_test
                WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
                DISCOVERY_TIMEOUT 120
                TEST_PREFIX "functional_")
add_dependencies(all_functional_tests phys_trigger_volume_test)

# ---------------------------------------------------------------------------
# Benchmarks (google/benchmark). Separate aggregate — never part of ctest or
# all_tests; run via the anim_bench binary or the run_bench_smoke target.
# ---------------------------------------------------------------------------
find_package(benchmark QUIET)
if(benchmark_FOUND)
        message(STATUS "Found system google/benchmark")
else()
        message(STATUS "google/benchmark not found, fetching via FetchContent...")
        include(FetchContent)
        set(BENCHMARK_ENABLE_TESTING      OFF CACHE INTERNAL "")
        set(BENCHMARK_ENABLE_GTEST_TESTS  OFF CACHE INTERNAL "")
        set(BENCHMARK_ENABLE_INSTALL      OFF CACHE INTERNAL "")
        set(BENCHMARK_ENABLE_WERROR       OFF CACHE INTERNAL "")
        set(BENCHMARK_USE_BUNDLED_GTEST   OFF CACHE INTERNAL "")
        FetchContent_Declare(benchmark
                        GIT_REPOSITORY https://github.com/google/benchmark.git
                        GIT_TAG        v1.9.4
                        GIT_SHALLOW    TRUE)
        FetchContent_MakeAvailable(benchmark)
endif()

add_custom_target(all_bench)

add_executable(anim_bench
                "${CMAKE_CURRENT_SOURCE_DIR}/bench/BenchMain.cpp"
                "${CMAKE_CURRENT_SOURCE_DIR}/bench/AnimBench.cpp")
target_include_directories(anim_bench PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/bench")
target_link_libraries(anim_bench
                benchmark::benchmark_main
                SECommon
                SE)
add_dependencies(all_bench anim_bench)

# Fast sanity run (seconds) producing the JSON artifact for baseline diffs.
add_custom_target(run_bench_smoke
                COMMAND $<TARGET_FILE:anim_bench>
                                --benchmark_min_time=0.05s
                                --benchmark_format=json
                                --benchmark_out=${CMAKE_BINARY_DIR}/anim_bench.json
                WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
                VERBATIM
                COMMENT "anim_bench smoke run -> anim_bench.json")

# Physics benchmarks: same conventions as anim_bench.
add_executable(phys_bench
                "${CMAKE_CURRENT_SOURCE_DIR}/bench/BenchMain.cpp"
                "${CMAKE_CURRENT_SOURCE_DIR}/bench/PhysicsBench.cpp")
target_include_directories(phys_bench PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/bench")
target_link_libraries(phys_bench
                benchmark::benchmark_main
                SECommon
                SE)
add_dependencies(all_bench phys_bench)

add_custom_target(run_phys_bench_smoke
                COMMAND $<TARGET_FILE:phys_bench>
                                --benchmark_min_time=0.05s
                                --benchmark_format=json
                                --benchmark_out=${CMAKE_BINARY_DIR}/phys_bench.json
                WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
                VERBATIM
                COMMENT "phys_bench smoke run -> phys_bench.json")

#TODO add custom_command for resource processing
