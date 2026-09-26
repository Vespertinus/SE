
set(TEMPLATE_EXPORT_SOURCES
        main.cpp
        TemplateExporter.cpp
)

add_executable(template_export ${TEMPLATE_EXPORT_SOURCES})

target_include_directories(template_export
        PRIVATE
                ${CMAKE_CURRENT_SOURCE_DIR}
                ${CMAKE_SOURCE_DIR}/generated
)

find_package(spdlog QUIET)
if(spdlog_FOUND)
        target_link_libraries(template_export spdlog::spdlog)
endif()
