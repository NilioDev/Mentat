add_executable(mtt_tests
    test_main.cpp
    test_dat_archive.cpp
    test_hsq.cpp
    test_game_files.cpp
    test_gfx.cpp
    test_hnm.cpp
    test_text_sound.cpp
    test_engine.cpp
    test_pointer.cpp
    test_music.cpp
    test_scene.cpp
    test_display.cpp
    test_characters.cpp
    test_play.cpp
    test_map.cpp
    test_talk.cpp
    test_saves.cpp
)
target_link_libraries(mtt_tests PRIVATE mentat::core)
if(TARGET mtt_mt32)
    target_link_libraries(mtt_tests PRIVATE mtt_mt32)
endif()
if(TARGET mtt_megacd)
    target_link_libraries(mtt_tests PRIVATE mtt_megacd)
endif()
target_compile_definitions(mtt_tests PRIVATE MENTAT_GAMEDATA_DIR="${PROJECT_SOURCE_DIR}/gamedata")
mtt_set_warnings(mtt_tests)

add_test(NAME mtt_tests COMMAND mtt_tests)
