cmake_minimum_required(VERSION 4.1.2)

project(keyhole LANGUAGES CXX)

add_executable(keyhole main.cpp)
target_compile_features(keyhole PRIVATE cxx_std_23)
target_compile_options(keyhole PRIVATE -Werror -Wall -Wextra -pedantic)
