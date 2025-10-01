cmake_minimum_required(VERSION 3.20)
project(foo_apple_music LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(FOOBAR2000_SDK_DIR C:/ServerFrameworkV2/test-foobarcomp/sdk)
include(cmake/foobar2000_sdk.cmake)

if(NOT TARGET foobar2000_sdk)
    message(FATAL_ERROR "foobar2000_sdk target was not created. Ensure FOOBAR2000_SDK_DIR is set before including foobar2000_sdk.cmake")
endif()

add_library(foo_apple_music SHARED src/foo_apple_music.cpp)
target_link_libraries(foo_apple_music PRIVATE foobar2000_sdk)
