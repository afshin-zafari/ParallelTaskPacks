
cmake_minimum_required(VERSION 3.20)

project(PersistentKernelPoC LANGUAGES CXX CUDA)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

file(GLOB LIB_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cu"
)

list(FILTER LIB_SOURCES EXCLUDE REGEX ".*/.*[Tt]est[^/]*\\.cpp$")

# ---- Static library ----
add_library(PersistentLib STATIC ${LIB_SOURCES})

target_include_directories(PersistentLib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        /usr/local/cuda-13.4/include
)

set_target_properties(PersistentLib PROPERTIES
    CUDA_STANDARD 17
    CUDA_STANDARD_REQUIRED ON
    CUDA_SEPARABLE_COMPILATION ON
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
