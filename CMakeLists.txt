# Following CMAKE tutorial from https://www.youtube.com/watch?v=Uq7Hwt18s3s

cmake_minimum_required(VERSION 3.22)

project(AudioPluginStarter)

set(CMAKE_CXX_STANDARD 23)

set(LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libs)
include(cmake/cpm.cmake)

CPMAddPackage(
    NAME JUCE
    GITHUB_REPOSITORY juce-framework/JUCE
    GIT_TAG 7.0.9
    VERSION 7.0.9
    SOURCE_DIR ${LIB_DIR}/juce
)

if (MSVC)
    add_compile_options(/W4 /WX /openmp)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_subdirectory(plugin)