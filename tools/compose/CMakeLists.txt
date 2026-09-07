set(COMPOSE_INC_DIRS)
set(COMPOSE_LIBS)

if(NOT TARGET nlohmann_json::nlohmann_json)
    find_package(nlohmann_json 3.11.0 QUIET)
    if(NOT nlohmann_json_FOUND)
        include(FetchContent)
        FetchContent_Declare(
            nlohmann_json
            GIT_REPOSITORY https://github.com/nlohmann/json.git
            GIT_TAG        v3.11.3
            GIT_SHALLOW    TRUE
        )
        set(JSON_BuildTests OFF CACHE INTERNAL "")
        set(JSON_Install    OFF CACHE INTERNAL "")
        FetchContent_MakeAvailable(nlohmann_json)
    endif()
endif()
list(APPEND COMPOSE_LIBS nlohmann_json::nlohmann_json)

find_package(spdlog REQUIRED)
list(APPEND COMPOSE_LIBS spdlog::spdlog)

find_package(Boost 1.54.0 REQUIRED COMPONENTS program_options filesystem)
list(APPEND COMPOSE_LIBS ${Boost_LIBRARIES})

include_directories(${Boost_INCLUDE_DIRS})

set(SOURCES
    main.cpp
    ManifestReader.cpp
    SceneComposer.cpp
)

add_executable(compose_scene ${SOURCES})

target_link_libraries(compose_scene ${COMPOSE_LIBS})
