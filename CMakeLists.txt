cmake_minimum_required(VERSION 3.22)

project(Melange
    VERSION 0.0.1
    DESCRIPTION "Clean-room engine that plays the 1992 desert adventure-strategy game from your own files"
    LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(MLG_BUILD_APP "Build the game executable (needs SDL3)" ON)
option(MLG_BUILD_TESTS "Build the unit tests" ${PROJECT_IS_TOP_LEVEL})
option(MLG_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(MlgWarnings)

# Engine core: platform independent, no SDL. Everything that can be tested
# without a screen lives here.
add_subdirectory(src/core)

if(MLG_BUILD_APP)
    include(MlgSDL3)
    add_subdirectory(src/app)
endif()

if(MLG_BUILD_TESTS AND NOT ANDROID)
    enable_testing()
    add_subdirectory(tests)
endif()
