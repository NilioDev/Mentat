# Provides mtt_z80 (Ivan Kosarev's Z80 emulator, MIT, one header) and
# mtt_nuked_opn2 (Nuke.YKT's YM2612 emulator Nuked-OPN2, LGPL 2.1 or later),
# downloaded at the pinned commits below, for the Mega CD music. That music
# also needs the player's own Mega CD disc at run time.

set(MTT_Z80_COMMIT "51d07188fd4ababfb465848b66ca65cc102b92cd" CACHE STRING "Z80 emulator commit")
set(MTT_NUKED_OPN2_COMMIT "335747d78cb0abbc3b55b004e62dad9763140115" CACHE STRING "Nuked-OPN2 commit")

include(FetchContent)
message(STATUS "Fetching the Z80 and YM2612 emulators for the Mega CD music")
# SOURCE_SUBDIR points where there is no CMakeLists.txt: only the sources
# are wanted, built below.
FetchContent_Declare(z80emu
    GIT_REPOSITORY https://github.com/kosarev/z80.git
    GIT_TAG ${MTT_Z80_COMMIT}
    SOURCE_SUBDIR mentat-sources-only)
FetchContent_Declare(nukedopn2
    GIT_REPOSITORY https://github.com/nukeykt/Nuked-OPN2.git
    GIT_TAG ${MTT_NUKED_OPN2_COMMIT}
    SOURCE_SUBDIR mentat-sources-only)
FetchContent_MakeAvailable(z80emu nukedopn2)

add_library(mtt_z80 INTERFACE)
target_include_directories(mtt_z80 SYSTEM INTERFACE "${z80emu_SOURCE_DIR}")

add_library(mtt_nuked_opn2 STATIC "${nukedopn2_SOURCE_DIR}/ym3438.c")
target_include_directories(mtt_nuked_opn2 SYSTEM PUBLIC "${nukedopn2_SOURCE_DIR}")
set_target_properties(mtt_nuked_opn2 PROPERTIES POSITION_INDEPENDENT_CODE ON)
