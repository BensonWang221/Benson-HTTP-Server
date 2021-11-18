aux_source_directory(. SOURCEFILES)

add_library(httpserver STATIC ${SOURCEFILES})

target_include_directories(httpserver PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../include)

target_link_libraries(httpserver PUBLIC pthread)