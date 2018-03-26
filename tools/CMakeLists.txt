
if(TOOLS)
        add_subdirectory("convert")
        if (GPU)
                add_subdirectory("scene_viewer")
        endif()        
endif()
