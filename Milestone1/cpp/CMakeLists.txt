cmake_minimum_required(VERSION 3.12)
project(ecowatt_cpp LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(ecowatt
  src/main.cpp
)
target_include_directories(ecowatt PRIVATE src src/sim)
if (MSVC)
  target_compile_options(ecowatt PRIVATE /W4)
else()
  target_compile_options(ecowatt PRIVATE -Wall -Wextra -Wpedantic)
endif()