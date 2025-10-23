cmake_minimum_required(VERSION 3.10) # declare min version reqd
project(SystemMonitorBackend)

set(CMAKE_CXX_STANDARD 17) # need c++ v17
set(CMAKE_CXX_STANDARD_REQUIRED ON) # enforce

# set default build configurations
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

add_executable(system_monitor
    src/main.cpp
    src/cpu.cpp
    src/memory.cpp
    src/processes.cpp
    src/disk.cpp
    src/network.cpp
    src/battery.cpp
    src/cli.cpp) # define executable

target_include_directories(system_monitor PRIVATE include)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(system_monitor PRIVATE
    -O3 # optimization flag
    
    -Wall
    -Wextra
    -Wpedantic
    
    -Wshadow # warn about variable shadowing
    -Wnon-virtual-dtor # warn about missing virtual destructors
    -Wold-style-cast  # warn about C-style casts
    -Wcast-align    # warn about pointer casts that increase alignment
    -Wunused          # warn about unused variables/functions
    -Woverloaded-virtual # warn about hidden overloaded virtual functions
  )
endif()