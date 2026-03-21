set(AUDIOIMPORT_INC_DIRS)
set(AUDIOIMPORT_LIBRARIES_LIST)

### nlohmann_json (same pattern as convert tool)
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
list(APPEND AUDIOIMPORT_LIBRARIES_LIST nlohmann_json::nlohmann_json)

### spdlog
find_package(spdlog REQUIRED)
list(APPEND AUDIOIMPORT_LIBRARIES_LIST spdlog::spdlog)

### dr_libs (WAV / MP3 / FLAC)
find_path(DR_LIBS_INCLUDE_DIR dr_wav.h
          PATHS "${CMAKE_SOURCE_DIR}/third_party/dr_libs")
if(NOT DR_LIBS_INCLUDE_DIR)
        message(FATAL_ERROR "audio_import: dr_libs not found in third_party/dr_libs/")
endif()
list(APPEND AUDIOIMPORT_INC_DIRS ${DR_LIBS_INCLUDE_DIR})

### stb (stb_vorbis for OGG)
find_path(STB_INCLUDE_DIR stb/stb_vorbis.h)
if(NOT STB_INCLUDE_DIR)
        message(FATAL_ERROR "audio_import: stb_vorbis.h not found — install libstb-dev")
endif()
list(APPEND AUDIOIMPORT_INC_DIRS ${STB_INCLUDE_DIR})

### libopus (encoder + decoder)
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
        pkg_check_modules(OPUS opus)
endif()
if(NOT OPUS_FOUND)
        find_path(OPUS_INCLUDE_DIR opus/opus.h
                  PATHS /usr/include /usr/local/include ~/usr/include)
        find_library(OPUS_LIBRARY NAMES opus
                     PATHS /usr/lib /usr/lib64 /usr/local/lib ~/usr/lib
                           /usr/lib/x86_64-linux-gnu)
        if(OPUS_INCLUDE_DIR AND OPUS_LIBRARY)
                set(OPUS_FOUND TRUE)
                set(OPUS_INCLUDE_DIRS ${OPUS_INCLUDE_DIR})
                set(OPUS_LIBRARIES    ${OPUS_LIBRARY})
        else()
                message(FATAL_ERROR
                        "audio_import: libopus headers not found.\n"
                        "  Install with: sudo apt install libopus-dev\n"
                        "  Include: ${OPUS_INCLUDE_DIR}, Library: ${OPUS_LIBRARY}")
        endif()
endif()
list(APPEND AUDIOIMPORT_INC_DIRS       ${OPUS_INCLUDE_DIRS})
list(APPEND AUDIOIMPORT_LIBRARIES_LIST ${OPUS_LIBRARIES})

### libsoxr (resampler)
if(PkgConfig_FOUND)
        pkg_check_modules(SOXR soxr)
endif()
if(NOT SOXR_FOUND)
        find_path(SOXR_INCLUDE_DIR soxr.h
                  PATHS /usr/include /usr/local/include ~/usr/include)
        find_library(SOXR_LIBRARY NAMES soxr
                     PATHS /usr/lib /usr/lib64 /usr/local/lib ~/usr/lib
                           /usr/lib/x86_64-linux-gnu)
        if(SOXR_INCLUDE_DIR AND SOXR_LIBRARY)
                set(SOXR_FOUND TRUE)
                set(SOXR_INCLUDE_DIRS ${SOXR_INCLUDE_DIR})
                set(SOXR_LIBRARIES    ${SOXR_LIBRARY})
        else()
                message(FATAL_ERROR
                        "audio_import: libsoxr headers not found.\n"
                        "  Install with: sudo apt install libsoxr-dev\n"
                        "  Include: ${SOXR_INCLUDE_DIR}, Library: ${SOXR_LIBRARY}")
        endif()
endif()
list(APPEND AUDIOIMPORT_INC_DIRS       ${SOXR_INCLUDE_DIRS})
list(APPEND AUDIOIMPORT_LIBRARIES_LIST ${SOXR_LIBRARIES})

### FlatBuffers (header-only runtime)
find_path(FLATBUFFERS_INCLUDE_DIR flatbuffers/flatbuffers.h)
if(NOT FLATBUFFERS_INCLUDE_DIR)
        message(FATAL_ERROR "audio_import: flatbuffers/flatbuffers.h not found.")
endif()
list(APPEND AUDIOIMPORT_INC_DIRS ${FLATBUFFERS_INCLUDE_DIR})

###
include_directories(
        ${AUDIOIMPORT_INC_DIRS}
        ${CMAKE_SOURCE_DIR}/generated
        ${CMAKE_SOURCE_DIR}/misc
)

set(SOURCES
        main.cpp
        Decoder.cpp
        Lufs.cpp
        Resampler.cpp
        Encoder.cpp
        Writer.cpp
        Manifest.cpp
)

add_executable(audio_import ${SOURCES})

target_link_libraries(audio_import
        ${AUDIOIMPORT_LIBRARIES_LIST}
)
