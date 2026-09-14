cmake_minimum_required(VERSION 3.22)
project(DarwinSim VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(DARWINSIM_BUILD_DISTRIBUTED "Build gRPC worker/controller" ON)
option(DARWINSIM_BUILD_TESTS "Build tests" ON)

find_package(Threads REQUIRED)

add_library(darwinsim_core
    src/core/genome.cpp
    src/core/rng.cpp
    src/core/phenotype.cpp
    src/core/world.cpp
    src/core/spatial_grid.cpp
    src/core/decision.cpp
    src/core/evolution.cpp
    src/core/hardware.cpp
    src/core/simulation.cpp
    src/core/checkpoint.cpp
)
target_include_directories(darwinsim_core PUBLIC include)
target_link_libraries(darwinsim_core PUBLIC Threads::Threads)
target_compile_options(darwinsim_core PRIVATE -Wall -Wextra -Wpedantic)

add_library(darwinsim_config src/config/config.cpp)
target_include_directories(darwinsim_config PUBLIC include)
target_link_libraries(darwinsim_config PUBLIC darwinsim_core)

add_executable(darwinsim-cli src/cli/main.cpp)
target_link_libraries(darwinsim-cli PRIVATE darwinsim_core darwinsim_config)

if(DARWINSIM_BUILD_DISTRIBUTED)
    find_package(Protobuf REQUIRED)
    find_package(gRPC CONFIG REQUIRED)
    find_package(Boost REQUIRED)
    find_package(SQLite3 REQUIRED)

      find_program(PROTOC_EXECUTABLE protoc REQUIRED)
      find_program(GRPC_CPP_PLUGIN_EXECUTABLE grpc_cpp_plugin REQUIRED)

      set(PROTO_FILE ${CMAKE_CURRENT_SOURCE_DIR}/proto/darwinsim.proto)
      set(GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
      file(MAKE_DIRECTORY ${GENERATED_DIR})

      set(PROTO_SRCS ${GENERATED_DIR}/darwinsim.pb.cc)
      set(PROTO_HDRS ${GENERATED_DIR}/darwinsim.pb.h)
      set(GRPC_SRCS ${GENERATED_DIR}/darwinsim.grpc.pb.cc)
      set(GRPC_HDRS ${GENERATED_DIR}/darwinsim.grpc.pb.h)

      add_custom_command(
          OUTPUT ${PROTO_SRCS} ${PROTO_HDRS} ${GRPC_SRCS} ${GRPC_HDRS}
          COMMAND ${PROTOC_EXECUTABLE}
              --proto_path=${CMAKE_CURRENT_SOURCE_DIR}/proto
              --cpp_out=${GENERATED_DIR}
              --grpc_out=${GENERATED_DIR}
              --plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN_EXECUTABLE}
              ${PROTO_FILE}
          DEPENDS ${PROTO_FILE}
          VERBATIM
      )

      add_library(darwinsim_rpc ${PROTO_SRCS} ${GRPC_SRCS} src/net/rpc_conversion.cpp)
      target_include_directories(darwinsim_rpc PUBLIC include ${GENERATED_DIR})
      target_link_libraries(darwinsim_rpc PUBLIC darwinsim_core protobuf::libprotobuf gRPC::grpc++)

      add_executable(darwinsim-worker src/worker/main.cpp)
      target_include_directories(darwinsim-worker PRIVATE ${GENERATED_DIR})
      target_link_libraries(darwinsim-worker PRIVATE darwinsim_rpc)

    add_executable(darwinsim-controller
        src/controller/main.cpp
        src/controller/worker_pool.cpp
        src/controller/http_server.cpp
        src/controller/persistence.cpp
    )
    target_include_directories(darwinsim-controller PRIVATE ${GENERATED_DIR})
    target_link_libraries(darwinsim-controller PRIVATE
        darwinsim_core darwinsim_config darwinsim_rpc
        SQLite::SQLite3 Threads::Threads
    )
endif()

if(DARWINSIM_BUILD_TESTS)
    enable_testing()
    add_executable(darwinsim-core-tests tests/core_tests.cpp)
    target_link_libraries(darwinsim-core-tests PRIVATE darwinsim_core)
    add_test(NAME darwinsim-core-tests COMMAND darwinsim-core-tests)
endif()
