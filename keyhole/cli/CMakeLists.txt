project(keyhole-cli)

add_executable(keyhole-cli main.cpp bytecode.cpp logging.cpp)
target_compile_features(keyhole-cli PRIVATE cxx_std_23)
target_compile_options(keyhole-cli PRIVATE -Werror -Wall -Wextra -pedantic)

add_executable(keyhole-cli-test tests/test.cpp)
target_compile_features(keyhole-cli-test PRIVATE cxx_std_23)
target_compile_options(keyhole-cli-test PRIVATE -Werror -Wall -Wextra -pedantic)
target_link_libraries(keyhole-cli-test GTest::gtest GTest::gtest_main)

gtest_discover_tests(keyhole-cli-test)
