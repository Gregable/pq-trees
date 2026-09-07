cmake_minimum_required(VERSION 3.16)
project(pqtrees CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

option(PQTREES_SANITIZE "Build with AddressSanitizer and UBSan" OFF)
if(PQTREES_SANITIZE)
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
endif()

add_library(pqtree STATIC pqnode.cc pqtree.cc)
target_include_directories(pqtree PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(pqtree PRIVATE -Wall -Wextra)

add_executable(pqtest pqtest.cc)
target_link_libraries(pqtest PRIVATE pqtree)

add_executable(fuzztest fuzztest.cc)
target_link_libraries(fuzztest PRIVATE pqtree)

enable_testing()
add_test(NAME pqtest COMMAND pqtest)
add_test(NAME fuzz_small COMMAND fuzztest --max-leaves 12 --iterations 3000)
add_test(NAME fuzz_large COMMAND fuzztest --max-leaves 42 --iterations 2000)
