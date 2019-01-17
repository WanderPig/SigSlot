cmake_minimum_required(VERSION 3.10)
project(sigslot)

set (CMAKE_CXX_STANDARD 17)
add_compile_options(-fcoroutines-ts -stdlib=libc++)
add_link_options(-fcoroutines-ts -stdlib=libc++)

include_directories(.)
add_definitions(-DSIGSLOT_COROUTINES)
add_executable(example test.cc sigslot/sigslot.h)