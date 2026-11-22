add_executable(mtt_tests
    test_main.cpp
    test_dat_archive.cpp
    test_hsq.cpp
    test_game_files.cpp
    test_gfx.cpp
    test_hnm.cpp
    test_text_sound.cpp
    test_engine.cpp
    test_music.cpp
    test_scene.cpp
    test_display.cpp
    test_characters.cpp
    test_play.cpp
)
target_link_libraries(mtt_tests PRIVATE mentat::core)
target_compile_definitions(mtt_tests PRIVATE MENTAT_GAMEDATA_DIR="${PROJECT_SOURCE_DIR}/gamedata")
mtt_set_warnings(mtt_tests)

add_test(NAME mtt_tests COMMAND mtt_tests)
