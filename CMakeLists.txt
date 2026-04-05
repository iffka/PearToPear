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

# ---- библиотека workspace ----
add_library(pear_fs
    src/pear/fs/workspace.cpp
)

target_include_directories(pear_fs PUBLIC
    include
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
    CLI11::CLI11
)

# ---- gRPC и Protobuf ----
find_package(gRPC CONFIG REQUIRED)
find_package(Protobuf REQUIRED)

set(PROTO_DIR ${CMAKE_CURRENT_SOURCE_DIR}/proto)
set(PROTO_FILES ${PROTO_DIR}/p2p.proto)

set(GENERATED_PROTO_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${GENERATED_PROTO_DIR})

set(GENERATED_SRCS)
foreach(proto_file ${PROTO_FILES})
    get_filename_component(proto_we ${proto_file} NAME_WE)
    set(pb_src      ${GENERATED_PROTO_DIR}/${proto_we}.pb.cc)
    set(pb_hdr      ${GENERATED_PROTO_DIR}/${proto_we}.pb.h)
    set(grpc_src    ${GENERATED_PROTO_DIR}/${proto_we}.grpc.pb.cc)
    set(grpc_hdr    ${GENERATED_PROTO_DIR}/${proto_we}.grpc.pb.h)
    list(APPEND GENERATED_SRCS ${pb_src} ${grpc_src})

    add_custom_command(
        OUTPUT ${pb_src} ${pb_hdr} ${grpc_src} ${grpc_hdr}
        COMMAND ${Protobuf_PROTOC_EXECUTABLE}
        ARGS
            --cpp_out ${GENERATED_PROTO_DIR}
            --grpc_out ${GENERATED_PROTO_DIR}
            -I ${PROTO_DIR}
            --plugin=protoc-gen-grpc=${gRPC_CPP_PLUGIN}
            ${proto_file}
        DEPENDS ${proto_file}
    )
endforeach()

# ---- библиотека pear_net ----
add_library(pear_net
    src/pear/net/node.cpp
    src/pear/net/master_service.cpp
    src/pear/net/storage_service.cpp
    src/pear/net/remote_client.cpp
    ${GENERATED_SRCS}
)

target_include_directories(pear_net PUBLIC
    include
    ${GENERATED_PROTO_DIR}
)

target_link_libraries(pear_net
    PUBLIC
    gRPC::grpc++
    protobuf::libprotobuf
)

# ---- добавить линковку к основному приложению pear ----
target_link_libraries(pear PRIVATE pear_net)

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