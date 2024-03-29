

#check boost
find_package(Boost 1.54.0 REQUIRED COMPONENTS system filesystem)
list(APPEND EXTERN_INC_DIRS ${Boost_INCLUDE_DIRS})
list(APPEND LIBRARIES_LIST ${Boost_LIBRARIES})

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

#check OpenCV
#imgproc only for workaround to convert BGR2BGRA
find_package(OpenCV REQUIRED COMPONENTS core highgui imgproc imgcodecs)
message(STATUS "Found OpenCV libraries: ${OpenCV_LIBRARIES}")
list(APPEND EXTERN_INC_DIRS ${OpenCV_INCLUDE_DIRS})
list(APPEND LIBRARIES_LIST ${OpenCV_LIBRARIES})

function(link_X11)
        find_package(X11 REQUIRED COMPONENTS Xxf86vm)
        if(X11_xf86vmode_FOUND AND X11_FOUND)
                message(STATUS "Found X11 library: ${X11_Xxf86vm_LIB}, ${X11_LIBRARIES}")
                set(EXTERN_INC_DIRS ${EXTERN_INC_DIRS} ${X11_xf86vmode_INCLUDE_PATH} ${X11_INCLUDE_DIR} PARENT_SCOPE)
                set(LIBRARIES_LIST ${LIBRARIES_LIST} ${X11_Xxf86vm_LIB} ${X11_LIBRARIES} PARENT_SCOPE)
                else()
                        message(FATAL_ERROR "Failed to found Xxf86vm or X11 library")
                endif()
        #        find_package(X11 REQUIRED COMPONENTS X11 Xxf86vm)
        #        if(X11_xf86vmode_FOUND AND X11_FOUND)
                #                message(STATUS "Found X11 library: ${X11_Xxf86vm_LIB}")
                #                set(EXTERN_INC_DIRS ${EXTERN_INC_DIRS} ${X11_xf86vmode_INCLUDE_PATH} ${X11_INCLUDE_DIR} PARENT_SCOPE)
                #                set(LIBRARIES_LIST ${LIBRARIES_LIST} ${X11_Xxf86vm_LIB} ${X11_LIBRARIES} PARENT_SCOPE)
                #        else()
                #                message(FATAL_ERROR "Failed to found Xxf86vm or X11 library")
                #        endif()

        find_package(SDL2 REQUIRED)
        if (SDL2_INCLUDE_DIRS AND SDL2_LIBRARIES)
                message(STATUS "Found SDL2 library: ${SDL2_INCLUDE_DIRS}")
                set(EXTERN_INC_DIRS ${EXTERN_INC_DIRS} ${SDL2_INCLUDE_DIRS} PARENT_SCOPE)
                set(LIBRARIES_LIST ${LIBRARIES_LIST} ${SDL2_LIBRARIES} PARENT_SCOPE)
        else()
                message(FATAL_ERROR "Failed to found SDL2 library")
        endif()

endfunction()


list(APPEND LIBRARIES_LIST "m")


