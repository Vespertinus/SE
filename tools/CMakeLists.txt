
if(TOOLS)
        add_subdirectory("convert")
        add_subdirectory("audio_import")
        if (GPU)
                add_subdirectory("scene_viewer")
        endif()
endif()
