set(MTT_APP_SOURCES
    main.cpp
)

if(ANDROID)
    # SDLActivity loads the game as libmain.so.
    add_library(mtt_app SHARED ${MTT_APP_SOURCES})
    set_target_properties(mtt_app PROPERTIES OUTPUT_NAME main)
else()
    add_executable(mtt_app WIN32 MACOSX_BUNDLE ${MTT_APP_SOURCES})
    set_target_properties(mtt_app PROPERTIES
        OUTPUT_NAME Mentat
        MACOSX_BUNDLE_GUI_IDENTIFIER io.github.niliodev.mentat
        MACOSX_BUNDLE_BUNDLE_NAME Mentat
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION})
endif()

target_link_libraries(mtt_app PRIVATE mentat::core SDL3::SDL3)
if(TARGET mtt_mt32)
    target_link_libraries(mtt_app PRIVATE mtt_mt32)
endif()
mtt_set_warnings(mtt_app)
