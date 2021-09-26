
function(find_header_library LIB_NAME HEADER_FILE SEARCH_PATH)
        find_path(      INC_DIR_${LIB_NAME}
                        NAMES "${HEADER_FILE}"
                        PATHS "${SEARCH_PATH}"
                        )

        if (INC_DIR_${LIB_NAME})
                message(STATUS "Found ${LIB_NAME} at ${INC_DIR_${LIB_NAME}}")
                set(EXTERN_INC_DIRS ${EXTERN_INC_DIRS} ${INC_DIR_${LIB_NAME}} PARENT_SCOPE)
        else()
                message(FATAL_ERROR "Failed to locate header '${HEADER_FILE}', check ${LIB_NAME} installation path")
        endif()
endfunction()

find_header_library("glm"               glm/glm.hpp "")
find_header_library("loki"              loki/Typelist.h "")

find_header_library("spdlog"            spdlog/spdlog.h "")
