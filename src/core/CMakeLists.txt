configure_file(version.hpp.in "${CMAKE_CURRENT_BINARY_DIR}/generated/melange/version.hpp" @ONLY)

add_library(mlg_core STATIC
    src/game_files.cpp
    src/data/dat_archive.cpp
    src/data/hsq.cpp
)
add_library(melange::core ALIAS mlg_core)

target_include_directories(mlg_core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_compile_features(mlg_core PUBLIC cxx_std_20)
set_target_properties(mlg_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
mlg_set_warnings(mlg_core)
