# 1. Define the minimum CMake version required
cmake_minimum_required(VERSION 3.15)

# 2. Set the project name and language (CXX is C++)
project(tennis_watch LANGUAGES CXX)

# 3. Enforce the modern C++23 standard (required for std::string_view, etc.)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 4. Define the executable and the source files to compile
add_executable(serve_watch
    src/main.cpp
    # src/ingestion/csv_parser.cpp
)

# 5. Tell CMake where to look for header files (the "include" folder)
# This makes #include "tennis_watch/core/point.hpp" work perfectly.
target_include_directories(serve_watch PUBLIC include)
