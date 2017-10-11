
function(prepend_path INPUT_LIST OUTPUT_LIST OUT_PATH)

        foreach(ELEM ${${INPUT_LIST}})
                list(APPEND RES_LIST "${OUT_PATH}/${ELEM}")
        endforeach()
        SET(${OUTPUT_LIST} ${RES_LIST} PARENT_SCOPE)
endfunction()

