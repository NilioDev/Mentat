configure_file(version.hpp.in "${CMAKE_CURRENT_BINARY_DIR}/generated/mentat/version.hpp" @ONLY)

add_library(mtt_core STATIC
    src/game_files.cpp
    src/audio/music.cpp
    src/audio/music_driver.cpp
    src/audio/opl.cpp
    src/data/dat_archive.cpp
    src/data/hsq.cpp
    src/data/hnm.cpp
    src/data/program_file.cpp
    src/data/program_tables.cpp
    src/engine/display.cpp
    src/engine/fiber.cpp
    src/engine/game.cpp
    src/engine/hnm_player.cpp
    src/engine/intro.cpp
    src/engine/actors.cpp
    src/engine/attack_view.cpp
    src/engine/places.cpp
    src/engine/resources.cpp
    src/engine/talker.cpp
    src/engine/videos.cpp
    src/data/string_table.cpp
    src/data/voc.cpp
    src/gfx/font.cpp
    src/gfx/palette.cpp
    src/gfx/portrait.cpp
    src/gfx/sprite.cpp
    src/gfx/sprite_sheet.cpp
    src/gfx/surface.cpp
    src/gfx/png.cpp
    src/gfx/scene.cpp
)
add_library(mentat::core ALIAS mtt_core)

# Nuked OPL3 (LGPL 2.1+), the FM chip emulator. Third-party code: built
# without our warning settings.
add_library(mtt_opl3 STATIC "${PROJECT_SOURCE_DIR}/third_party/nuked-opl3/opl3.c")
target_include_directories(mtt_opl3 PUBLIC "${PROJECT_SOURCE_DIR}/third_party/nuked-opl3")
set_target_properties(mtt_opl3 PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_link_libraries(mtt_core PRIVATE mtt_opl3)

target_include_directories(mtt_core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_compile_features(mtt_core PUBLIC cxx_std_20)
set_target_properties(mtt_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
mtt_set_warnings(mtt_core)
