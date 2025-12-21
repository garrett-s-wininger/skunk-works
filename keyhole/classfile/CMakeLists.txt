project(kh-classfile)

add_library(
    kh-classfile
    SHARED
    classfile.cpp
    constant_pool.cpp
    parsing.cpp
    reader.cpp
    sinks.cpp)

target_compile_features(kh-classfile PRIVATE cxx_std_23)
target_compile_options(kh-classfile PRIVATE -Werror -Wall -Wextra -pedantic)
target_include_directories(kh-classfile PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(
    kh-classfile-test
    tests/classfile.cpp
    tests/constant_pool.cpp
    tests/parsing.cpp
    tests/serialization.cpp)

target_compile_features(kh-classfile-test PRIVATE cxx_std_23)
target_compile_options(kh-classfile-test PRIVATE -Werror -Wall -Wextra -pedantic)
target_link_libraries(kh-classfile-test kh-classfile GTest::gtest GTest::gmock_main)

gtest_discover_tests(kh-classfile-test)
