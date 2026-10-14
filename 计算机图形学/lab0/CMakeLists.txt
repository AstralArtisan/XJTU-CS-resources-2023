cmake_minimum_required(VERSION 3.11)
project(optimizer VERSION 0.1)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

add_subdirectory(deps/fmt)

set(SOURCES
    main.cpp
)

add_executable(${PROJECT_NAME} ${SOURCES})
target_include_directories(${PROJECT_NAME} 
    PRIVATE ${CMAKE_SOURCE_DIR}/deps
)
target_compile_definitions(${PROJECT_NAME}
    PRIVATE SPDLOG_FMT_EXTERNAL
    PRIVATE FMT_HEADER_ONLY
)
target_link_libraries(${PROJECT_NAME} fmt::fmt)