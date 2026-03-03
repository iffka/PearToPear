cmake_minimum_required(VERSION 3.16)
project(PearToPear LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---- библиотека с workspace ----
add_library(pear_fs
    src/pear/fs/workspace.cpp
)

target_include_directories(pear_fs PUBLIC
    include
)

# ---- тесты ----
enable_testing()

find_package(GTest REQUIRED)

add_executable(workspace_test
    tests/pear/fs/workspace_test.cpp
)

target_link_libraries(workspace_test PRIVATE
    pear_fs
    GTest::gtest
    GTest::gtest_main
)

add_test(NAME workspace_test COMMAND workspace_test)