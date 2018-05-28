
set(TOOLS_INC_DIRS)
set(TOOLS_LIBRARIES_LIST)

### check dependencies
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
                "FlatBuffersMeshWriter.cpp"
                "FBXReader.cpp"
                "FlatBuffersSceneTreeWriter.cpp")

add_executable(         convert
                        ${SOURCES})

target_link_libraries(  convert
                        ${TOOLS_LIBRARIES_LIST}
                        )
