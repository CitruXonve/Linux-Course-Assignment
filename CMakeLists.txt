cmake_minimum_required(VERSION 3.8)
project(ls_cmd)

set(CMAKE_CXX_STANDARD 98)

set(SOURCE_FILES main.c)
add_executable(ls_cmd ${SOURCE_FILES})