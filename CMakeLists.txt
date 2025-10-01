cmake_minimum_required(VERSION 3.24)
project(foo_apple_music VERSION 0.1.0 LANGUAGES CXX)

option(FOO_APPLE_MUSIC_BUILD_TESTS "Build standalone tests" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT FOOBAR2000_SDK_DIR)
    message(FATAL_ERROR "FOOBAR2000_SDK_DIR must point to the root of the foobar2000 SDK")
endif()

set(FOOBAR2000_COMPONENT_NAME "foo_apple_music")

set(SRC
    src/component_entry.cpp
    src/apple_music_auth.cpp
    src/apple_music_client.cpp
    src/apple_music_input.cpp
    src/playlist_sync_service.cpp
    src/preferences.cpp
    src/apple_music_login_dialog.cpp
    res/foo_apple_music.rc
)

set(INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_library(${FOOBAR2000_COMPONENT_NAME} SHARED ${SRC})

target_include_directories(${FOOBAR2000_COMPONENT_NAME}
    PRIVATE
        ${INCLUDE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/res
        ${FOOBAR2000_SDK_DIR}/SDK
        ${FOOBAR2000_SDK_DIR}/foobar2000/ATLHelpers
)

target_compile_definitions(${FOOBAR2000_COMPONENT_NAME}
    PRIVATE
        NOMINMAX
        _WIN32_WINNT=0x0A00
        WIN32_LEAN_AND_MEAN
        FOOBAR2000_TARGET_VERSION=80
)

add_subdirectory(${FOOBAR2000_SDK_DIR}/SDK ${CMAKE_BINARY_DIR}/foobar2000_sdk EXCLUDE_FROM_ALL)

target_link_libraries(${FOOBAR2000_COMPONENT_NAME}
    PRIVATE
        foobar2000_sdk
        nlohmann_json::nlohmann_json
        libcurl::libcurl
        Bcrypt
        Crypt32
        Winhttp
        Shlwapi
)

set_target_properties(${FOOBAR2000_COMPONENT_NAME}
    PROPERTIES
        OUTPUT_NAME ${FOOBAR2000_COMPONENT_NAME}
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

if(MSVC)
    target_compile_options(${FOOBAR2000_COMPONENT_NAME}
        PRIVATE /permissive- /W4 /wd4100 /wd4201 /wd4324)
endif()

if(FOO_APPLE_MUSIC_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
