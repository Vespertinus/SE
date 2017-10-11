

#check boost
find_package(Boost 1.54.0 REQUIRED COMPONENTS system filesystem)
list(APPEND EXTERN_INC_DIRS ${Boost_INCLUDE_DIRS})
list(APPEND LIBRARIES_LIST ${Boost_LIBRARIES})

#check OpenCV
find_package(OpenCV REQUIRED COMPONENTS core highgui)
message(STATUS "Found OpenCV libraries: ${OpenCV_LIBRARIES}")
list(APPEND EXTERN_INC_DIRS ${OpenCV_INCLUDE_DIRS})
list(APPEND LIBRARIES_LIST ${OpenCV_LIBRARIES})

if (GPU)
        #check OpenGL
        find_package(OpenGL)
        list(APPEND EXTERN_INC_DIRS ${OPENGL_INCLUDE_DIR})
        list(APPEND LIBRARIES_LIST ${OPENGL_gl_LIBRARY})

        #check OIS library support for interacting with input devices
        find_path(      OIS_INCLUDE_DIR
                        "ois/OISMouse.h")
        find_library(   OIS_LIBRARY
                        NAMES OIS
                        )
        if(OIS_INCLUDE_DIR AND OIS_LIBRARY)
                message(STATUS "Found OIS library at ${OIS_LIBRARY}")
                list(APPEND EXTERN_INC_DIRS ${OIS_INCLUDE_DIR})
                list(APPEND LIBRARIES_LIST ${OIS_LIBRARY})
        else()
                message(FATAL_ERROR "Failed to found OIS library")
        endif()


else()
        #check OSMesa lib for software rendering. Mesa must be build with openswr or llvmpipe support
        find_path(      OSMESA_INCLUDE_DIR
                        "GL/osmesa.h")
        find_library(   OSMESA_LIBRARY
                        NAMES OSMesa glapi
                        )
        if(OSMESA_INCLUDE_DIR AND OSMESA_LIBRARY)
                message(STATUS "Found OSMesa library at ${OSMESA_LIBRARY}")
                list(APPEND EXTERN_INC_DIRS ${OSMESA_INCLUDE_DIR})
                list(APPEND LIBRARIES_LIST ${OSMESA_LIBRARY})
        else()
                message(FATAL_ERROR "Failed to found OSMesa library")
        endif()
endif()



function(link_X11)
        find_package(X11 REQUIRED COMPONENTS X11 Xxf86vm)
        if(X11_xf86vmode_FOUND AND X11_FOUND)
                message(STATUS "Found X11 library: ${X11_Xxf86vm_LIB}")
                list(APPEND EXTERN_INC_DIRS ${X11_xf86vmode_INCLUDE_PATH} ${X11_INCLUDE_DIR})
                list(APPEND LIBRARIES_LIST ${X11_Xxf86vm_LIB} ${X11_LIBRARIES})
        else()
                message(FATAL_ERROR "Failed to found Xxf86vm or X11 library")
        endif()
endfunction()


#header only packages:
find_package(spdlog CONFIG REQUIRED)
message(STATUS "Found spdlog package, settings at '${spdlog_DIR}'")
list(APPEND LIBRARIES_LIST spdlog::spdlog)


list(APPEND LIBRARIES_LIST "m")


