cmake_minimum_required(VERSION 3.20)
project(chess-engine VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(ENGINE_NATIVE "Optimize for the build machine (-march=native)" ON)
option(ENGINE_BUILD_TESTS "Build the GoogleTest suite" ON)

add_library(engine_core STATIC
  src/bitboard.cpp
  src/position.cpp
  src/movegen.cpp
  src/eval.cpp
  src/tt.cpp
  src/search.cpp
  src/uci.cpp
  src/bench.cpp
)
target_include_directories(engine_core PUBLIC src)

find_package(Threads REQUIRED)
target_link_libraries(engine_core PUBLIC Threads::Threads)

if(MSVC)
  target_compile_options(engine_core PUBLIC /W4)
else()
  target_compile_options(engine_core PUBLIC -Wall -Wextra -Wpedantic)
  if(ENGINE_NATIVE)
    include(CheckCXXCompilerFlag)
    check_cxx_compiler_flag("-march=native" HAS_MARCH_NATIVE)
    if(HAS_MARCH_NATIVE)
      target_compile_options(engine_core PUBLIC -march=native)
    endif()
  endif()
endif()

add_executable(chess-engine src/main.cpp)
target_link_libraries(chess-engine PRIVATE engine_core)

if(ENGINE_BUILD_TESTS)
  enable_testing()
  find_package(GTest QUIET)
  if(NOT GTest_FOUND)
    include(FetchContent)
    FetchContent_Declare(
      googletest
      URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.tar.gz
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
  endif()

  add_executable(engine_tests
    tests/test_main.cpp
    tests/perft_test.cpp
    tests/position_test.cpp
    tests/movegen_test.cpp
    tests/eval_test.cpp
    tests/search_test.cpp
  )
  target_link_libraries(engine_tests PRIVATE engine_core GTest::gtest)
  include(GoogleTest)
  gtest_discover_tests(engine_tests DISCOVERY_TIMEOUT 60)
endif()
