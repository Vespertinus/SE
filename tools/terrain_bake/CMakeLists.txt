set(TERRAIN_BAKE_INC_DIRS)
set(TERRAIN_BAKE_LIBS)

# 16-bit PNG input via stb_image (same resolution as tools/convert)
find_path(STB_INCLUDE_DIR stb/stb_image.h)
if(NOT STB_INCLUDE_DIR)
    include(FetchContent)
    FetchContent_Declare(
        stb
        GIT_REPOSITORY https://github.com/nothings/stb.git
        GIT_TAG        master
        GIT_SHALLOW    TRUE
    )
    FetchContent_Populate(stb)
    set(STB_INCLUDE_DIR ${stb_SOURCE_DIR})
endif()
list(APPEND TERRAIN_BAKE_INC_DIRS ${STB_INCLUDE_DIR})

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
list(APPEND TERRAIN_BAKE_LIBS nlohmann_json::nlohmann_json)

find_package(spdlog REQUIRED)
list(APPEND TERRAIN_BAKE_LIBS spdlog::spdlog)

find_package(Boost 1.54.0 REQUIRED COMPONENTS program_options filesystem)
list(APPEND TERRAIN_BAKE_LIBS ${Boost_LIBRARIES})

include_directories(${TERRAIN_BAKE_INC_DIRS})
include_directories(${Boost_INCLUDE_DIRS})

add_executable(terrain_bake main.cpp StbImpl.cpp)
target_link_libraries(terrain_bake ${TERRAIN_BAKE_LIBS})
