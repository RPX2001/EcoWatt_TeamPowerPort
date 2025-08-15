cmake_minimum_required(VERSION 3.12)
project(ecowatt_cpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Collect all source files
set(SRC_FILES
    src/main.cpp

    # Application
    src/core/application/Coordinator.cpp

    # Peripheral/Driver
    src/core/peripheral/Acquisition.cpp
    src/core/peripheral/Uploader.cpp
    src/core/peripheral/Timers.cpp

    # HAL
    src/sim/InverterSim.cpp

    # Sim
    src/sim/CloudStub.cpp
)

add_executable(ecowatt ${SRC_FILES})

# Include directories
target_include_directories(ecowatt PRIVATE
    include
    include/core
    include/core/peripheral
    include/core/application
    include/sim
)

# Compiler warnings
if(MSVC)
    target_compile_options(ecowatt PRIVATE /W4)
else()
    target_compile_options(ecowatt PRIVATE -Wall -Wextra -Wpedantic)
endif()
