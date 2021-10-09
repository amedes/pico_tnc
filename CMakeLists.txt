cmake_minimum_required(VERSION 3.12)

include(pico_sdk_import.cmake)

project(pico_examples C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

set(PICO_EXAMPLES_PATH ${PROJECT_SOURCE_DIR})

pico_sdk_init()

include(example_auto_set_url.cmake)

add_subdirectory(pico_tnc)
