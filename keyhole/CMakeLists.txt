cmake_minimum_required(VERSION 3.31.0)
project(keyhole)

enable_language(CXX)
enable_testing()

include(FetchContent)
include(GoogleTest)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.17.0
)

FetchContent_MakeAvailable(googletest)

add_subdirectory(classfile)
add_subdirectory(cli)
