cmake_minimum_required(VERSION 3.20)

project(LogServer
    VERSION 1.0.0
    DESCRIPTION "Log server"
    LANGUAGES CXX
)

# Block in-source builds (keeps the tree clean)
if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
    message(FATAL_ERROR "In-source builds are not allowed. Build in a separate directory, e.g. cmake -B build")
endif()

# Sensible default build type for single-config generators
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
        Debug Release RelWithDebInfo MinSizeRel)
endif()

add_executable(log_server
    main.cpp
    messageframer.cpp
    server.cpp
)

# Per-target standard instead of the global variable
target_compile_features(log_server PRIVATE cxx_std_17)

target_include_directories(log_server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Warnings on, compiler-aware
if(MSVC)
    target_compile_options(log_server PRIVATE /W4 /permissive-)
else()
    target_compile_options(log_server PRIVATE
        -Wall -Wextra -Wpedantic -Wshadow -Wconversion)
endif()

# Servers almost always need threads
find_package(Threads REQUIRED)
target_link_libraries(log_server PRIVATE Threads::Threads)