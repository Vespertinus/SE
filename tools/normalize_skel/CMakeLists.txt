
find_package(spdlog REQUIRED)
find_package(Boost 1.54.0 REQUIRED COMPONENTS program_options filesystem)

find_path(FLATBUFFERS_INCLUDE_DIR flatbuffers/flatbuffers.h)
if(NOT FLATBUFFERS_INCLUDE_DIR)
        message(FATAL_ERROR "normalize_skel: flatbuffers headers not found")
endif()

include_directories(${Boost_INCLUDE_DIRS} ${FLATBUFFERS_INCLUDE_DIR})

set(SOURCES
        main.cpp
        NormalizeSkel.cpp
)

add_executable(normalize_skel ${SOURCES})

target_link_libraries(normalize_skel
        spdlog::spdlog
        ${Boost_LIBRARIES}
)
