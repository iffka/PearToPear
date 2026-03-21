cmake_minimum_required(VERSION 3.10)
project(PearToPeer)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

list(APPEND CMAKE_PREFIX_PATH /opt/homebrew)

find_package(gRPC CONFIG REQUIRED)

find_program(PROTOC_PROGRAM protoc
    PATHS /opt/homebrew/bin
    REQUIRED
)
if(NOT PROTOC_PROGRAM)
    message(FATAL_ERROR "protoc not found. Please install protobuf")
endif()

get_target_property(GRPC_CPP_PLUGIN gRPC::grpc_cpp_plugin LOCATION)


set(PROTO_DIR ${CMAKE_CURRENT_SOURCE_DIR}/proto)
set(PROTO_FILES ${PROTO_DIR}/request.proto)

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
        COMMAND ${PROTOC_PROGRAM}
        ARGS
            --cpp_out ${GENERATED_PROTO_DIR}
            --grpc_out ${GENERATED_PROTO_DIR}
            -I ${PROTO_DIR}
            --plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN}
            ${proto_file}
        DEPENDS ${proto_file}
    )
endforeach()

include_directories(${GENERATED_PROTO_DIR})

add_executable(server
    src/server.cpp
    ${GENERATED_SRCS}
)
target_link_libraries(server
    PRIVATE
    gRPC::grpc++
    protobuf::libprotobuf
)

add_executable(client
    src/client.cpp
    ${GENERATED_SRCS}
)
target_link_libraries(client
    PRIVATE
    gRPC::grpc++
    protobuf::libprotobuf
)


# Автоматическое создание двух тестовых окружений (node1 и node2)

set(TEST_FILES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/test_files)


add_custom_target(setup_nodes ALL

    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/node1
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/node2

    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:server> ${CMAKE_CURRENT_BINARY_DIR}/node1/
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:client> ${CMAKE_CURRENT_BINARY_DIR}/node1/
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:server> ${CMAKE_CURRENT_BINARY_DIR}/node2/
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:client> ${CMAKE_CURRENT_BINARY_DIR}/node2/

    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/node1/files
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/node2/files

    COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_BINARY_DIR}/node1/database.txt
    COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_BINARY_DIR}/node2/database.txt

    DEPENDS server client
)

