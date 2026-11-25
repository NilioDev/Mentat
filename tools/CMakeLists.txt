# Developer tools. They read the developer's own game files; what they write
# stays on the developer's machine and is never committed.
add_executable(mtt_dump dump.cpp)
set_target_properties(mtt_dump PROPERTIES OUTPUT_NAME mentat-dump)
target_link_libraries(mtt_dump PRIVATE mentat::core)
mtt_set_warnings(mtt_dump)

add_executable(mtt_music music.cpp)
set_target_properties(mtt_music PROPERTIES OUTPUT_NAME mentat-music)
target_link_libraries(mtt_music PRIVATE mentat::core)
if(TARGET mtt_megacd)
    target_link_libraries(mtt_music PRIVATE mtt_megacd)
endif()
mtt_set_warnings(mtt_music)

add_executable(mtt_scene scene.cpp)
set_target_properties(mtt_scene PROPERTIES OUTPUT_NAME mentat-scene)
target_link_libraries(mtt_scene PRIVATE mentat::core)
mtt_set_warnings(mtt_scene)

add_executable(mtt_run run.cpp)
set_target_properties(mtt_run PROPERTIES OUTPUT_NAME mentat-run)
target_link_libraries(mtt_run PRIVATE mentat::core)
mtt_set_warnings(mtt_run)
