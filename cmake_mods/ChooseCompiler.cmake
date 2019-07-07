
set(MIN_CXX_VERSION "6.1")

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS ${MIN_CXX_VERSION})
                message(FATAL_ERROR "Insufficient g++ version = '${CMAKE_CXX_COMPILER_VERSION}' but required at least '${MIN_CXX_VERSION}'")
        endif()
else()        
#message(FATAL_ERROR "Unsupported compiler (tested on GCC)")
endif()


if (NOT "${CMAKE_CXX_COMPILER_VERSION}" STREQUAL "${CMAKE_C_COMPILER_VERSION}")
        message(FATAL_ERROR "Compilers versions mismatch: C compiler version: ${CMAKE_C_COMPILER_VERSION}, C++ compiler version: ${CMAKE_CXX_COMPILER_VERSION}")
endif()

