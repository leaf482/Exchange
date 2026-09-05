cmake_minimum_required(VERSION 3.20)
project(mercury VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(mercury src/version.cpp)
target_include_directories(mercury PUBLIC include)

function(mercury_set_warnings target)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
  elseif(MSVC)
    target_compile_options(${target} PRIVATE /W4 /WX)
  endif()
endfunction()

mercury_set_warnings(mercury)

include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.15.2
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_WERROR OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
  googlebenchmark
  GIT_REPOSITORY https://github.com/google/benchmark.git
  GIT_TAG v1.9.1
)
FetchContent_MakeAvailable(googlebenchmark)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  if(TARGET benchmark)
    target_compile_options(benchmark PRIVATE -Wno-c2y-extensions)
  endif()
  if(TARGET benchmark_main)
    target_compile_options(benchmark_main PRIVATE -Wno-c2y-extensions)
  endif()
endif()

enable_testing()
add_subdirectory(tests)
add_subdirectory(benchmarks)
add_subdirectory(apps)

option(MERCURY_BUILD_PYTHON "Build optional pybind11 Engine bindings" OFF)
if(MERCURY_BUILD_PYTHON)
  add_subdirectory(python/bindings)
endif()
