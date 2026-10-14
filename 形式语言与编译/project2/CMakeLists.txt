cmake_minimum_required(VERSION 3.16)

project(QCompiler LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(qcompiler_lib
    src/arm64.cpp
    src/ir.cpp
    src/lexer.cpp
    src/parser.cpp
    src/qast.cpp
    src/sigma_dfa.cpp
    src/slr.cpp
)

target_include_directories(qcompiler_lib PUBLIC include)

if(MSVC)
    target_compile_options(qcompiler_lib PRIVATE /W4)
else()
    target_compile_options(qcompiler_lib PRIVATE -Wall -Wextra -pedantic)
endif()

add_executable(qcompiler src/main.cpp)
target_link_libraries(qcompiler PRIVATE qcompiler_lib)

add_executable(qcompiler_tests tests/test_qcompiler.cpp)
target_link_libraries(qcompiler_tests PRIVATE qcompiler_lib)

enable_testing()
add_test(NAME qcompiler_tests COMMAND qcompiler_tests)
add_test(
    NAME qcompiler_routes
    COMMAND ${CMAKE_COMMAND}
        -D QCOMPILER_EXE=$<TARGET_FILE:qcompiler>
        -D QCOMPILER_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/qsort.ql
        -D QCOMPILER_ROUTE_OUT=${CMAKE_CURRENT_BINARY_DIR}/route-tests/qsort
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_routes.cmake
)
