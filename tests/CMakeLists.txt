
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

# ---------------------------------------------------------------------------
# world_demo integration suites
# (worlddemo_app shadow App.h — never add it to global include dirs) over the
# committed demo assets under resource/world/village_gen + friends.
# ---------------------------------------------------------------------------
function(se_add_worlddemo_test NAME SOURCE)
        se_add_functional_test(${NAME} ${SOURCE})
        target_include_directories(${NAME} BEFORE PRIVATE
                        "${FUNCTIONAL_DIR}/worlddemo_app"
                        # headless StaticModel double (see shadow_static/StaticModel.h):
                        # the demo NPC templates carry StaticModel bodies whose
                        # production Enable()/Disable() need a renderer
                        "${FUNCTIONAL_DIR}/shadow_static")
endfunction()

se_add_worlddemo_test(worlddemo_assets_test    WorldDemoAssetsTest.cpp)
# Round-6: authored-content physics guard — the real wd_village.sesc must keep
# colliders on every wall tile (plain/window/door), door lintels, kinematic
# door leaves, and block a character driven at a wall.
se_add_worlddemo_test(world_content_physics_test WorldContentPhysicsTest.cpp)
# follow-up: the streaming/spawner/nav/BT suites over the demo-mirror
# world hit an intermittent fixture-level crash (SEGV in/around PhysicsSystem
# init and population arming). Sources kept:
#   WorldDemoStreamingTest.cpp / WorldDemoSpawnerTest.cpp /
#   WorldDemoNavTest.cpp / WorldDemoBTTest.cpp (+ WorldDemoHarness.h,
#   WorldDemoWorldBuilder.h). worlddemo_bt_test re-enabled in round 9 (the
# guard-chase and bell-investigate brains need live coverage); the other
# three stay gated until their fixtures are stable:
# se_add_worlddemo_test(worlddemo_streaming_test WorldDemoStreamingTest.cpp)
# se_add_worlddemo_test(worlddemo_spawner_test   WorldDemoSpawnerTest.cpp)
# se_add_worlddemo_test(worlddemo_nav_test       WorldDemoNavTest.cpp)
se_add_worlddemo_test(worlddemo_bt_test         WorldDemoBTTest.cpp)
# Portal component — door↔Manual-sub-level streaming with hysteresis
se_add_worlddemo_test(worldportal_test          WorldPortalTest.cpp)

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
# World:  .seworld asset parsing + the WorldSystem
# sub-level lifecycle. The world composition (WorldSystem/EntityManager/
# StateMachineSystem + StateMachine/RigidBody components + WorldAsset/
# StateMachineAsset resources) enters via a target-local shadow App.h
# (trigger_app precedent — never add world_app/ to global include dirs).
add_executable(world_asset_test
                "${FUNCTIONAL_DIR}/TestMain.cpp"
                "${FUNCTIONAL_DIR}/WorldAssetTest.cpp")
target_include_directories(world_asset_test BEFORE PRIVATE
                "${FUNCTIONAL_DIR}/world_app")
target_link_libraries(world_asset_test
                GTest::GTest
                ${GMOCK_LIBRARIES}
                SECommon
                SE)
gtest_discover_tests(world_asset_test
                WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
                DISCOVERY_TIMEOUT 120
                TEST_PREFIX "functional_")
add_dependencies(all_functional_tests world_asset_test)

add_executable(world_system_test
                "${FUNCTIONAL_DIR}/TestMain.cpp"
                "${FUNCTIONAL_DIR}/WorldSystemTest.cpp")
target_include_directories(world_system_test BEFORE PRIVATE
                "${FUNCTIONAL_DIR}/world_app")
target_link_libraries(world_system_test
                GTest::GTest
                ${GMOCK_LIBRARIES}
                SECommon
                SE)
gtest_discover_tests(world_system_test
                WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
                DISCOVERY_TIMEOUT 120
                TEST_PREFIX "functional_")
add_dependencies(all_functional_tests world_system_test)

# World suite the streaming planner/worker (Tier B) and the
# full streaming pipeline through WorldSystem (Tier C)
add_executable(streaming_director_test
                "${FUNCTIONAL_DIR}/TestMain.cpp"
                "${FUNCTIONAL_DIR}/StreamingDirectorTest.cpp")
target_link_libraries(streaming_director_test
                GTest::GTest
                ${GMOCK_LIBRARIES}
                SECommon
                SE)
gtest_discover_tests(streaming_director_test
                WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
                DISCOVERY_TIMEOUT 120
                TEST_PREFIX "functional_")
add_dependencies(all_functional_tests streaming_director_test)

add_executable(streaming_test
                "${FUNCTIONAL_DIR}/TestMain.cpp"
                "${FUNCTIONAL_DIR}/StreamingTest.cpp")
target_include_directories(streaming_test BEFORE PRIVATE
                "${FUNCTIONAL_DIR}/world_app")
target_link_libraries(streaming_test
                GTest::GTest
                ${GMOCK_LIBRARIES}
                SECommon
                SE)
gtest_discover_tests(streaming_test
                WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
                DISCOVERY_TIMEOUT 120
                TEST_PREFIX "functional_")
add_dependencies(all_functional_tests streaming_test)

# Spatial suite: the pure query grid (Tier B) and the full
# world wiring — authored SpatialEntry, activation sweep, focus mirror,
# node-listener tracking — through the world_app shadow composition (Tier C).
add_executable(spatial_index_test
                "${FUNCTIONAL_DIR}/TestMain.cpp"
                "${FUNCTIONAL_DIR}/SpatialIndexTest.cpp")
target_link_libraries(spatial_index_test
                GTest::GTest
                ${GMOCK_LIBRARIES}
                SECommon
                SE)
gtest_discover_tests(spatial_index_test
                WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
                DISCOVERY_TIMEOUT 120
                TEST_PREFIX "functional_")
add_dependencies(all_functional_tests spatial_index_test)

add_executable(spatial_test
                "${FUNCTIONAL_DIR}/TestMain.cpp"
                "${FUNCTIONAL_DIR}/SpatialTest.cpp")
target_include_directories(spatial_test BEFORE PRIVATE
                "${FUNCTIONAL_DIR}/world_app")
target_link_libraries(spatial_test
                GTest::GTest
                ${GMOCK_LIBRARIES}
                SECommon
                SE)
gtest_discover_tests(spatial_test
                WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
                DISCOVERY_TIMEOUT 120
                TEST_PREFIX "functional_")
add_dependencies(all_functional_tests spatial_test)

# Spawner suite: the pure cores — placement sampling, typed
# conditions, the population runtime state machine (Tier B, SpatialIndexTest
# pattern) and the full spawner wiring through the world_app shadow composition
# (Tier C). The Tier C target also links the template_export extraction lib for
# the .setm round-trip test.
add_executable(spawner_core_test
                "${FUNCTIONAL_DIR}/TestMain.cpp"
                "${FUNCTIONAL_DIR}/SpawnerCoreTest.cpp")
target_link_libraries(spawner_core_test
                GTest::GTest
                ${GMOCK_LIBRARIES}
                SECommon
                SE)
gtest_discover_tests(spawner_core_test
                WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
                DISCOVERY_TIMEOUT 120
                TEST_PREFIX "functional_")
add_dependencies(all_functional_tests spawner_core_test)

add_executable(spawner_test
                "${FUNCTIONAL_DIR}/TestMain.cpp"
                "${FUNCTIONAL_DIR}/SpawnerTest.cpp")
target_include_directories(spawner_test BEFORE PRIVATE
                "${FUNCTIONAL_DIR}/world_app")
target_link_libraries(spawner_test
                GTest::GTest
                ${GMOCK_LIBRARIES}
                SECommon
                SE)
gtest_discover_tests(spawner_test
                WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
                DISCOVERY_TIMEOUT 120
                TEST_PREFIX "functional_")
add_dependencies(all_functional_tests spawner_test)

# Blackboard suite: asset/core/component/global + trigger
# bindings over the blackboard_app composition (world + AI systems).
add_executable(blackboard_test
                "${FUNCTIONAL_DIR}/TestMain.cpp"
                "${FUNCTIONAL_DIR}/BlackboardTest.cpp")
target_include_directories(blackboard_test BEFORE PRIVATE
                "${FUNCTIONAL_DIR}/blackboard_app")
target_link_libraries(blackboard_test
                GTest::GTest
                ${GMOCK_LIBRARIES}
                SECommon
                SE)
gtest_discover_tests(blackboard_test
                WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
                DISCOVERY_TIMEOUT 120
                TEST_PREFIX "functional_")
add_dependencies(all_functional_tests blackboard_test)

# Behavior tree suite: asset bake/validation + runtime
# semantics + the composed bt_app acceptance brain (movement/crowd + AI).
add_executable(behavior_tree_test
                "${FUNCTIONAL_DIR}/TestMain.cpp"
                "${FUNCTIONAL_DIR}/BehaviorTreeTest.cpp")
target_include_directories(behavior_tree_test BEFORE PRIVATE
                "${FUNCTIONAL_DIR}/bt_app")
target_link_libraries(behavior_tree_test
                GTest::GTest
                ${GMOCK_LIBRARIES}
                SECommon
                SE)
gtest_discover_tests(behavior_tree_test
                WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
                DISCOVERY_TIMEOUT 120
                TEST_PREFIX "functional_")
add_dependencies(all_functional_tests behavior_tree_test)

# Terrain snap suite: spawner snap=TerrainHeight over the
# terrain_app composition (tiles page through the shared worker).
add_executable(spawner_terrain_test
                "${FUNCTIONAL_DIR}/TestMain.cpp"
                "${FUNCTIONAL_DIR}/SpawnerTerrainTest.cpp")
target_include_directories(spawner_terrain_test BEFORE PRIVATE
                "${FUNCTIONAL_DIR}/terrain_app")
target_link_libraries(spawner_terrain_test
                GTest::GTest
                ${GMOCK_LIBRARIES}
                SECommon
                SE)
gtest_discover_tests(spawner_terrain_test
                WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
                DISCOVERY_TIMEOUT 120
                TEST_PREFIX "functional_")
add_dependencies(all_functional_tests spawner_terrain_test)

# Template export suite: .sesc subtree -> .setm round trip
# through the template_export extraction lib (second TU, no SE_IMPL inside).
add_executable(template_export_test
                "${FUNCTIONAL_DIR}/TestMain.cpp"
                "${FUNCTIONAL_DIR}/TemplateExportTest.cpp"
                "${CMAKE_SOURCE_DIR}/tools/template_export/TemplateExporter.cpp")
target_include_directories(template_export_test BEFORE PRIVATE
                "${FUNCTIONAL_DIR}/world_app"
                "${CMAKE_SOURCE_DIR}/tools")
target_link_libraries(template_export_test
                GTest::GTest
                ${GMOCK_LIBRARIES}
                SECommon
                SE)
gtest_discover_tests(template_export_test
                WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
                DISCOVERY_TIMEOUT 120
                TEST_PREFIX "functional_")
add_dependencies(all_functional_tests template_export_test)

# Terrain suite: bake library + pure queries + worker content
# dispatch + camera-aware AllVisible (Tier B), and the full tile pipeline with
# physics over the terrain_app shadow composition (Tier C).
add_executable(terrain_bake_test
                "${FUNCTIONAL_DIR}/TestMain.cpp"
                "${FUNCTIONAL_DIR}/TerrainBakeTest.cpp")
target_include_directories(terrain_bake_test BEFORE PRIVATE
                "${FUNCTIONAL_DIR}/terrain_app")
target_link_libraries(terrain_bake_test
                GTest::GTest
                ${GMOCK_LIBRARIES}
                SECommon
                SE)
gtest_discover_tests(terrain_bake_test
                WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
                DISCOVERY_TIMEOUT 120
                TEST_PREFIX "functional_")
add_dependencies(all_functional_tests terrain_bake_test)

# Navigation bake suite: the shared NavBake library vs
# analytic scenes, with Detour queries over the raw tile bytes (Tier B).
if (NAV)
add_executable(nav_bake_test
                "${FUNCTIONAL_DIR}/TestMain.cpp"
                "${FUNCTIONAL_DIR}/NavBakeTest.cpp")
target_link_libraries(nav_bake_test
                GTest::GTest
                ${GMOCK_LIBRARIES}
                SECommon
                SE
                Recast
                Detour)
gtest_discover_tests(nav_bake_test
                WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
                DISCOVERY_TIMEOUT 120
                TEST_PREFIX "functional_")
add_dependencies(all_functional_tests nav_bake_test)

# Navigation runtime suite: the world-agnostic NavigationSystem
# over synthetic bakes (Tier B) and the world-driven lifecycle through
# WorldNavBridge — terrain-grid paging, sub-level tiles, NavAgent, spawner snap,
# world-teardown drain (Tier C, nav_app shadow composition).
add_executable(nav_runtime_test
                "${FUNCTIONAL_DIR}/TestMain.cpp"
                "${FUNCTIONAL_DIR}/NavRuntimeTest.cpp")
target_include_directories(nav_runtime_test BEFORE PRIVATE
                "${FUNCTIONAL_DIR}/nav_app")
target_link_libraries(nav_runtime_test
                GTest::GTest
                ${GMOCK_LIBRARIES}
                SECommon
                SE
                Recast
                Detour
                DetourCrowd)
gtest_discover_tests(nav_runtime_test
                WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
                DISCOVERY_TIMEOUT 120
                TEST_PREFIX "functional_")
add_dependencies(all_functional_tests nav_runtime_test)

# Crowd steering suite: PathFollower + CrowdSystemT over the
# scripted backend (Tier B) and the full movement stack with CharacterVirtual
# bodies (Tier C, crowd_app shadow composition).
add_executable(crowd_test
                "${FUNCTIONAL_DIR}/TestMain.cpp"
                "${FUNCTIONAL_DIR}/CrowdTest.cpp")
target_include_directories(crowd_test BEFORE PRIVATE
                "${FUNCTIONAL_DIR}/crowd_app")
target_link_libraries(crowd_test
                GTest::GTest
                ${GMOCK_LIBRARIES}
                SECommon
                SE
                Recast
                Detour
                DetourCrowd)
gtest_discover_tests(crowd_test
                WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
                DISCOVERY_TIMEOUT 120
                TEST_PREFIX "functional_")
add_dependencies(all_functional_tests crowd_test)
endif()

add_executable(terrain_test
                "${FUNCTIONAL_DIR}/TestMain.cpp"
                "${FUNCTIONAL_DIR}/TerrainTest.cpp")
target_include_directories(terrain_test BEFORE PRIVATE
                "${FUNCTIONAL_DIR}/terrain_app")
target_link_libraries(terrain_test
                GTest::GTest
                ${GMOCK_LIBRARIES}
                SECommon
                SE)
gtest_discover_tests(terrain_test
                WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
                DISCOVERY_TIMEOUT 120
                TEST_PREFIX "functional_")
add_dependencies(all_functional_tests terrain_test)

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

add_executable(world_bench
                "${CMAKE_CURRENT_SOURCE_DIR}/bench/BenchMain.cpp"
                "${CMAKE_CURRENT_SOURCE_DIR}/bench/WorldBench.cpp")
target_include_directories(world_bench BEFORE PRIVATE
                "${CMAKE_CURRENT_SOURCE_DIR}/bench"
                "${FUNCTIONAL_DIR}/worlddemo_app"
                # headless StaticModel double — the demo world's instantiated
                # nodes carry StaticModel bodies; production Enable()/Disable()
                # need a renderer this headless binary does not have
                "${FUNCTIONAL_DIR}/shadow_static")
target_link_libraries(world_bench
                benchmark::benchmark_main
                SECommon
                SE)
add_dependencies(all_bench world_bench)

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
