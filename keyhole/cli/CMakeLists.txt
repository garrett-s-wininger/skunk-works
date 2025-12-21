project(kh-cli)

add_executable(kh-cli main.cpp logging.cpp)
target_compile_features(kh-cli PRIVATE cxx_std_23)
target_compile_options(kh-cli PRIVATE -Werror -Wall -Wextra -pedantic)
target_link_libraries(kh-cli PRIVATE kh-classfile)
