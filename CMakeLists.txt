cmake_minimum_required(VERSION 3.20)
project(VAULT VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -pthread")

find_package(OpenSSL REQUIRED)

get_filename_component(CP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Protocol/CP" ABSOLUTE)
if(NOT EXISTS "${CP_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "CP library not found at ${CP_DIR}")
endif()

add_subdirectory(${CP_DIR} ${CMAKE_BINARY_DIR}/cp_build)

add_library(vault_core STATIC
    src/object_store.cpp
    src/sic.cpp
    src/version_chain.cpp
    src/delta.cpp
    src/name_registry.cpp
    src/author_registry.cpp
)
target_link_libraries(vault_core PUBLIC cp_shared OpenSSL::Crypto pthread)
target_include_directories(vault_core PUBLIC
    include
    ${CP_DIR}/shared
    ${CP_DIR}/receiver
)

add_executable(vault-receiver
    src/receiver/main.cpp
    src/receiver/receiver.cpp
    ${CP_DIR}/receiver/cp_receiver_node.cpp
)
target_link_libraries(vault-receiver vault_core cp_shared)
target_include_directories(vault-receiver PRIVATE ${CP_DIR}/shared ${CP_DIR}/receiver)

add_executable(vault
    src/sender/main.cpp
    src/sender/commands/hook.cpp
    src/sender/commands/get.cpp
    src/sender/commands/sic_cmd.cpp
    src/sender/commands/diff.cpp
    src/sender/commands/sync.cpp
    src/sender/commands/rollback.cpp
    src/sender/commands/verify.cpp
)
target_link_libraries(vault vault_core)

add_executable(vault-relay
    src/relay/main.cpp
    src/relay/relay.cpp
)
target_link_libraries(vault-relay OpenSSL::Crypto pthread)
target_include_directories(vault-relay PRIVATE include)

enable_testing()
add_subdirectory(tests)
