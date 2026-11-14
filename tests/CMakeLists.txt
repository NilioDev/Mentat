add_executable(mlg_tests
    test_main.cpp
    test_dat_archive.cpp
    test_hsq.cpp
    test_game_files.cpp
)
target_link_libraries(mlg_tests PRIVATE melange::core)
target_compile_definitions(mlg_tests PRIVATE MELANGE_GAMEDATA_DIR="${PROJECT_SOURCE_DIR}/gamedata")
mlg_set_warnings(mlg_tests)

add_test(NAME mlg_tests COMMAND mlg_tests)
