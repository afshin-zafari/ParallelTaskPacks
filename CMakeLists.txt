
cmake_minimum_required(VERSION 3.20)

project(PersistentKernelPoC LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

file(GLOB LIB_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

list(FILTER LIB_SOURCES EXCLUDE REGEX ".*/.*[Tt]est[^/]*\\.cpp$")

# ---- Static library ----
add_library(PersistentLib STATIC ${LIB_SOURCES})

target_include_directories(PersistentLib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# ---- GoogleTest ----
enable_testing()

find_package(GTest CONFIG REQUIRED)

add_executable(PersistentLibTests
    ParallelTaskPacks_gTests/test.cpp
    ParallelTaskPacks_gTests/pch.cpp
    ParallelTaskPacks_gTests/pch.h
)

target_link_libraries(PersistentLibTests
    PRIVATE
        PersistentLib
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(PersistentLibTests)
