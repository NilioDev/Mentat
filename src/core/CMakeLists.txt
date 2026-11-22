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
    src/engine/flat_map.cpp
    src/engine/map_screen.cpp
    src/engine/globe.cpp
    src/engine/globe_screen.cpp
    src/engine/hnm_player.cpp
    src/engine/intro.cpp
    src/engine/actors.cpp
    src/engine/attack_view.cpp
    src/engine/panel.cpp
    src/engine/places.cpp
    src/engine/play.cpp
    src/engine/resources.cpp
    src/engine/talker.cpp
    src/engine/text_box.cpp
    src/engine/videos.cpp
    src/engine/world.cpp
    src/engine/dialogue.cpp
    src/engine/saved_games.cpp
    src/engine/talk.cpp
    src/data/save_file.cpp
    src/data/string_table.cpp
    src/data/phrase_file.cpp
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

# The AdLib Gold's surround (YM7128B) and stereo (TDA8425) chips, BSD
# 2-Clause. Third-party code too.
add_library(mtt_gold STATIC
    "${PROJECT_SOURCE_DIR}/third_party/ym7128b/YM7128B_emu.c"
    "${PROJECT_SOURCE_DIR}/third_party/tda8425/TDA8425_emu.c")
target_include_directories(mtt_gold PUBLIC
    "${PROJECT_SOURCE_DIR}/third_party/ym7128b"
    "${PROJECT_SOURCE_DIR}/third_party/tda8425")
target_compile_definitions(mtt_gold PUBLIC TDA8425_USE_EXTENSIONS=0)
set_target_properties(mtt_gold PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(NOT MSVC)
    target_link_libraries(mtt_gold PRIVATE m)
endif()
target_link_libraries(mtt_core PRIVATE mtt_gold)

target_include_directories(mtt_core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_compile_features(mtt_core PUBLIC cxx_std_20)
set_target_properties(mtt_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
mtt_set_warnings(mtt_core)
