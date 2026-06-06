cmake_minimum_required(VERSION 3.20)
project(ngit VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(ngit src/main.cpp)

target_include_directories(ngit PRIVATE ${CMAKE_SOURCE_DIR}/src)
