
set(TOOLS_INC_DIRS)
set(TOOLS_LIBRARIES_LIST)

### check dependencies
find_header_library("tinyobjloader"     tiny_obj_loader.h "")

find_package(nlohmann_json 3.11.0)
if(NOT nlohmann_json_FOUND)
        message(STATUS "nlohmann_json not found, fetching via FetchContent...")
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
list(APPEND TOOLS_LIBRARIES_LIST nlohmann_json::nlohmann_json)

find_path(STB_INCLUDE_DIR stb_image.h PATH_SUFFIXES stb)
if(NOT STB_INCLUDE_DIR)
        message(STATUS "stb_image.h not found, fetching via FetchContent...")
        include(FetchContent)
        FetchContent_Declare(
                stb
                GIT_REPOSITORY https://github.com/nothings/stb.git
                GIT_TAG        master
                GIT_SHALLOW    TRUE
        )
        FetchContent_MakeAvailable(stb)
        set(STB_INCLUDE_DIR ${stb_SOURCE_DIR})
else()
        message(STATUS "Found stb_image.h at ${STB_INCLUDE_DIR}")
endif()
list(APPEND TOOLS_INC_DIRS ${STB_INCLUDE_DIR})

find_path(      TINYGLTF_INCLUDE_DIR
                "tiny_gltf.h"
                PATHS "${CMAKE_SOURCE_DIR}/third_party/tinygltf")
if(TINYGLTF_INCLUDE_DIR)
        message(STATUS "Found tinygltf at ${TINYGLTF_INCLUDE_DIR}")
        list(APPEND TOOLS_INC_DIRS ${TINYGLTF_INCLUDE_DIR})
else()
        message(FATAL_ERROR "Failed to find tiny_gltf.h in third_party/tinygltf/")
endif()

find_package(Boost 1.54.0 REQUIRED COMPONENTS program_options filesystem)
include_directories (${Boost_INCLUDE_DIRS})
list(APPEND TOOLS_LIBRARIES_LIST ${Boost_LIBRARIES})

find_path(      FBXSDK_INCLUDE_DIR
                "fbxsdk.h")

find_library(   FBXSDK_LIBRARY
                NAMES fbxsdk
                )

if(FBXSDK_INCLUDE_DIR AND FBXSDK_LIBRARY)
                message(STATUS "Found FBXSDK library at ${FBXSDK_LIBRARY}")
                list(APPEND TOOLS_INC_DIRS ${FBXSDK_INCLUDE_DIR})
        list(APPEND TOOLS_LIBRARIES_LIST ${FBXSDK_LIBRARY} "-ldl")
else()
        message(STATUS "lib: '${FBXSDK_LIBRARY}'")
        message(STATUS "inc: '${FBXSDK_INCLUDE_DIR}'")
        message(FATAL_ERROR "Failed to found FBXSDK library")
endif()
###

include_directories(${TOOLS_INC_DIRS})

set(SOURCES     "main.cpp"
                "Common.cpp"
                "OBJReader.cpp"
                "FBXReader.cpp"
                "GLTFReader.cpp"
                "FlatBuffersMeshWriter.cpp"
                "FlatBuffersComponentWriter.cpp"
                "FlatBuffersSceneTreeWriter.cpp")

add_executable(         convert
                        ${SOURCES})

target_link_libraries(  convert
                        ${TOOLS_LIBRARIES_LIST}
                        )
