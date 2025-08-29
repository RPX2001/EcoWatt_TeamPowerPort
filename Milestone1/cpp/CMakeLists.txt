cmake_minimum_required(VERSION 3.10)
project(InverterFirmware)

set(CMAKE_CXX_STANDARD 17)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/core/peripheral
    ${CMAKE_SOURCE_DIR}/include/core/application
    ${CMAKE_SOURCE_DIR}/include/sim
)

# Gather all source files
file(GLOB_RECURSE SOURCES
    ${CMAKE_SOURCE_DIR}/src/*.cpp
)

add_executable(ecowatt ${SOURCES})