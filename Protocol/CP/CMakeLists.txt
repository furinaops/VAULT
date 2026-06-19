cmake_minimum_required(VERSION 3.10)
project(cp_protocol)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")

find_package(OpenSSL REQUIRED)

add_subdirectory(shared)
add_subdirectory(sender)
add_subdirectory(receiver)
