# Provides the MT32Emu::mt32emu target: Munt's Roland MT-32 emulator
# (LGPL 2.1 or later), downloaded and built from the pinned release below.
# It needs the player's own MT-32 ROM files at run time; none come with it.

set(MTT_MUNT_TAG "libmt32emu_2_8_6" CACHE STRING "Munt release to build")

include(FetchContent)
message(STATUS "Building Munt's mt32emu ${MTT_MUNT_TAG} from source")
set(libmt32emu_SHARED OFF CACHE BOOL "" FORCE)
set(libmt32emu_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(libmt32emu_C_INTERFACE OFF CACHE BOOL "" FORCE)
set(libmt32emu_WITH_VERSION_TAGGING OFF CACHE BOOL "" FORCE)
FetchContent_Declare(munt
    GIT_REPOSITORY https://github.com/munt/munt.git
    GIT_TAG ${MTT_MUNT_TAG}
    GIT_SHALLOW TRUE
    SOURCE_SUBDIR mt32emu)
FetchContent_MakeAvailable(munt)
set_target_properties(mt32emu PROPERTIES POSITION_INDEPENDENT_CODE ON)
