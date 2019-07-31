
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)


add_library(SE INTERFACE )

add_library(SECommon SHARED ${COMMON_SOURCES})


set (                           INSTALLED_HEADERS_DIR "include/SE")
set (                           CONFIG_PACKAGE_LOCATION "${CMAKE_INSTALL_LIBDIR}/cmake/SimpleEngine")
prepend_path(                   LOCAL_INC_DIRS
                                ABS_LOCAL_INC_DIR
                                "${CMAKE_INSTALL_PREFIX}/${INSTALLED_HEADERS_DIR}")
prepend_path(                   THIRD_PARTY_INC_DIRS
                                ABS_THIRD_PARTY_INC_DIRS
                                "${CMAKE_INSTALL_PREFIX}/${INSTALLED_HEADERS_DIR}")

target_include_directories(     SE
                                INTERFACE
                                ${ABS_LOCAL_INC_DIR}
                                ${EXTERN_INC_DIRS}
                                ${ABS_THIRD_PARTY_INC_DIRS}
                                )

target_link_libraries(          SE
                                INTERFACE
                                ${LIBRARIES_LIST}
                                )

write_basic_package_version_file(
                                "${CMAKE_CURRENT_BINARY_DIR}/SimpleEngine/SimpleEngineConfigVersion.cmake"
                                VERSION ${Upstream_VERSION}
                                COMPATIBILITY AnyNewerVersion )



install(                        DIRECTORY ${LOCAL_INC_DIRS}
                                DESTINATION ${INSTALLED_HEADERS_DIR}
                                FILES_MATCHING
                                PATTERN "*.h"
                                PATTERN "*.tcc"
                                )

install(                        DIRECTORY ${THIRD_PARTY_INC_DIRS}
                                DESTINATION "${INSTALLED_HEADERS_DIR}/third_party"
                                FILES_MATCHING
                                PATTERN "*.h"
                                PATTERN "*.tcc"
                                )

install(                        TARGETS
                                SE
                                SECommon
                                DESTINATION ${CMAKE_INSTALL_LIBDIR}
                                EXPORT SimpleEngineTargets)

install(                        EXPORT SimpleEngineTargets
                                NAMESPACE SimpleEngine::
                                DESTINATION ${CONFIG_PACKAGE_LOCATION})


configure_file(cmake_mods/SimpleEngineConfig.cmake
        "${CMAKE_CURRENT_BINARY_DIR}/SimpleEngine/SimpleEngineConfig.cmake"
        COPYONLY
        )

install(
        FILES
        cmake_mods/SimpleEngineConfig.cmake
        "${CMAKE_CURRENT_BINARY_DIR}/SimpleEngine/SimpleEngineConfigVersion.cmake"
        DESTINATION
        ${CONFIG_PACKAGE_LOCATION}
        COMPONENT
        Devel
        )

