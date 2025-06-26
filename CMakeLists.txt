cmake_minimum_required(VERSION 3.10)
project(example)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Multi-platform support
if(MSVC)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

# Add source files
add_executable(example
    example.cpp
)

# Thread library
find_package(Threads REQUIRED)
target_link_libraries(example PRIVATE Threads::Threads)

# Optional: better cross-platform compatibility
if(APPLE)
    # macOS specific settings
elseif(UNIX)
    # Linux specific settings
elseif(WIN32)
    # Windows specific settings
endif()