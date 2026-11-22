# Developer tools. They read the developer's own game files; what they write
# stays on the developer's machine and is never committed.
add_executable(mtt_dump dump.cpp)
set_target_properties(mtt_dump PROPERTIES OUTPUT_NAME mentat-dump)
target_link_libraries(mtt_dump PRIVATE mentat::core)
mtt_set_warnings(mtt_dump)
