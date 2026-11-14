configure_file(version.hpp.in "${CMAKE_CURRENT_BINARY_DIR}/generated/mentat/version.hpp" @ONLY)

add_library(mtt_core STATIC
    src/game_files.cpp
    src/data/dat_archive.cpp
    src/data/hsq.cpp
)
add_library(mentat::core ALIAS mtt_core)

target_include_directories(mtt_core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_compile_features(mtt_core PUBLIC cxx_std_20)
set_target_properties(mtt_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
mtt_set_warnings(mtt_core)
