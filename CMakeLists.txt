cmake_minimum_required(VERSION 3.16)
project(PearToPear LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(PEAR_DEBUG "Enable Pear debug output" OFF)

include(FetchContent)

FetchContent_Declare(
    CLI11
    GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
    GIT_TAG v2.6.0
)

FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(CLI11 googletest)

find_package(SQLite3 REQUIRED)

# ---- библиотека workspace ----
add_library(pear_fs
    src/pear/fs/workspace.cpp
)

target_include_directories(pear_fs PUBLIC
    include
)

# ---- библиотека БД ----
# Публичный заголовок: include/pear/db/sqlite_database.hpp
# Реализует pear::net::DatabaseFacade поверх SQLite.
add_library(pear_db
    src/pear/db/sqlite.cpp
    src/pear/db/schema.cpp
    src/pear/db/sqlite_database.cpp
)

target_include_directories(pear_db PUBLIC
    include
)

target_link_libraries(pear_db PUBLIC
    SQLite::SQLite3
)

# ---- CLI приложение ----
add_executable(pear
    src/main.cpp
    src/pear/cli/commands.cpp
)

target_include_directories(pear PRIVATE
    include
)

target_link_libraries(pear PRIVATE
    pear_fs
    pear_db
    CLI11::CLI11
)

if(PEAR_DEBUG)
    target_compile_definitions(pear PRIVATE PEAR_DEBUG)
endif()

# ---- тесты ----
enable_testing()

add_executable(workspace_test
    tests/pear/fs/workspace_test.cpp
)

target_link_libraries(workspace_test PRIVATE
    pear_fs
    GTest::gtest
    GTest::gtest_main
)

add_test(NAME workspace_test COMMAND workspace_test)