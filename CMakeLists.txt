cmake_minimum_required(VERSION 3.20)
project(cpp_vulkan_journey CXX)

# c++23 - with fallback on c++20
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# -std=c++23, instead of -std=gnu+23
set(CMAKE_CXX_EXTENSIONS OFF)

# enable warnings - in C++ it's crucial
add_compile_options(
  -Wall       # main warnings
  -Wextra     # additional
  -Wpedantic  # strictly according to the standard
  -Wshadow    # catches variables shadowing
  -Wconversion    # implicit type narrowing (in C++ this is a silent killer)
)
add_executable(HelloWorld 01_HelloWorld/main.cpp)