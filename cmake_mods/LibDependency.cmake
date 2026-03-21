

#check boost
find_package(Boost 1.54.0 REQUIRED COMPONENTS system filesystem)
list(APPEND EXTERN_INC_DIRS ${Boost_INCLUDE_DIRS})
list(APPEND LIBRARIES_LIST ${Boost_LIBRARIES})

#set(CMAKE_FIND_DEBUG_MODE TRUE)
#set(CMAKE_FIND_DEBUG_MODE FALSE)

find_package(spdlog REQUIRED)
list(APPEND LIBRARIES_LIST spdlog::spdlog)

if (GPU)
        #check OpenGL
        if(POLICY CMP0072)
                cmake_policy(SET CMP0072 NEW)
        endif()

        find_package(OpenGL REQUIRED)
        list(APPEND EXTERN_INC_DIRS ${OPENGL_INCLUDE_DIRS})
        list(APPEND LIBRARIES_LIST OpenGL::OpenGL)
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

        find_package(SDL2 REQUIRED)
        if (SDL2_INCLUDE_DIRS AND SDL2_LIBRARIES)
                message(STATUS "Found SDL2 library: ${SDL2_INCLUDE_DIRS}")
                set(EXTERN_INC_DIRS ${EXTERN_INC_DIRS} ${SDL2_INCLUDE_DIRS} PARENT_SCOPE)
                set(LIBRARIES_LIST ${LIBRARIES_LIST} ${SDL2_LIBRARIES} PARENT_SCOPE)
        else()
                message(FATAL_ERROR "Failed to found SDL2 library")
        endif()

endfunction()


if(AUDIO)
        find_package(OpenAL REQUIRED)
        list(APPEND LIBRARIES_LIST OpenAL::OpenAL)
        message(STATUS "Found OpenAL: ${OPENAL_LIBRARY}")
endif()

list(APPEND LIBRARIES_LIST "m")


