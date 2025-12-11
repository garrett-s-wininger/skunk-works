cmake_minimum_required(VERSION 4.1.2)
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

add_executable(keyhole main.cpp bytecode.cpp logging.cpp)
target_compile_features(keyhole PRIVATE cxx_std_23)
target_compile_options(keyhole PRIVATE -Werror -Wall -Wextra -pedantic)

add_executable(keyhole-test test.cpp)
target_compile_features(keyhole-test PRIVATE cxx_std_23)
target_compile_options(keyhole-test PRIVATE -Werror -Wall -Wextra -pedantic)
target_link_libraries(keyhole-test GTest::gtest GTest::gtest_main)

gtest_discover_tests(keyhole-test)
