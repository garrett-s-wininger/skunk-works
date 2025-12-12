project(kh-cli)

add_executable(kh-cli main.cpp bytecode.cpp logging.cpp)
target_compile_features(kh-cli PRIVATE cxx_std_23)
target_compile_options(kh-cli PRIVATE -Werror -Wall -Wextra -pedantic)
