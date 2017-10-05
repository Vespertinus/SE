
cmake_minimum_required(VERSION 3.0.0)

project(SimpleEngine C CXX)

option(GPU                      "Compile with direct rendering support"  ON)
option(ASAN                     "Compile with address sanitizer"         OFF)


include_directories(            "${CMAKE_SOURCE_DIR}/core"
                                "${CMAKE_SOURCE_DIR}/common"
                                "${CMAKE_SOURCE_DIR}/units"
                                )

list(APPEND CMAKE_PREFIX_PATH "~/usr" "/usr/local")
                
link_directories(               /usr/lib
                                /usr/lib64
                                ~/usr/lib
                                )

set(CMAKE_MODULE_PATH           "${CMAKE_SOURCE_DIR}/cmake_mods"
                                "${CMAKE_MODULE_PATH}"
                                )

include("ChooseCompiler")
include("IncDependency")
include("LibDependency")

if (UNIX AND NOT APPLE)
        if (GPU)
                include_directories(    "${CMAKE_SOURCE_DIR}/X11")
                link_X11()
        endif()
else()
        message(FATAL_ERROR "OS unsupported yet")
endif()

set (COMMON_FLAGS               "-fno-strict-aliasing -pthread -ffast-math -mfpmath=sse -ggdb3 -Wall") # -Wextra

if (ASAN)
        set(COMMON_FLAGS        "${COMMON_FLAGS} -fsanitize=address")
endif()

set(COMMON_C_FLAGS              "-std=gnu11 -fgnu89-inline")
set(COMMON_CXX_FLAGS            "-std=gnu++14")

set(C_WARNING_FLAGS             "-Wignored-qualifiers -Wwrite-strings")
set(CXX_WARNING_FLAGS           "-Wmissing-declarations -Winvalid-pch")

add_definitions(-D_GNU_SOURCE -D_FILE_OFFSET_BITS=64)


set(CMAKE_C_FLAGS               "${COMMON_FLAGS} ${COMMON_C_FLAGS} ${C_WARNING_FLAGS}")
set(CMAKE_CXX_FLAGS             "${COMMON_FLAGS} ${COMMON_CXX_FLAGS} ${CXX_WARNING_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS      "${COMMON_FLAGS} -Wl,--hash-style=gnu")


set(DEBUG_FLAGS                 "-O0")
set(RELEASE_FLAGS               "-Og -finline-small-functions")

# Release build type
set(CMAKE_C_FLAGS_RELEASE       "${RELEASE_FLAGS}")
set(CMAKE_CXX_FLAGS_RELEASE     "${RELEASE_FLAGS}")
# Debug build type
set(CMAKE_C_FLAGS_DEBUG         "${DEBUG_FLAGS}")
set(CMAKE_CXX_FLAGS_DEBUG       "${DEBUG_FLAGS}")

if (NOT CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE Debug CACHE STRING
                "Build types: Debug(default) Release" FORCE)
endif()

set(EXECUTABLE_OUTPUT_PATH      "${CMAKE_SOURCE_DIR}")

add_subdirectory("samples")


