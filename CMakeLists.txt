cmake_minimum_required(VERSION 3.10)
project(powerls VERSION 1.0 LANGUAGES CXX)

# Force the compiler to use modern C++20 standards
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Define the final executable name and its source file
add_executable(powerls src/main.cpp)

# Install the binary to the standard bin directory
install(TARGETS powerls RUNTIME DESTINATION bin)