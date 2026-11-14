set(MLG_APP_SOURCES
    main.cpp
)

if(ANDROID)
    # SDLActivity loads the game as libmain.so.
    add_library(mlg_app SHARED ${MLG_APP_SOURCES})
    set_target_properties(mlg_app PROPERTIES OUTPUT_NAME main)
else()
    add_executable(mlg_app WIN32 MACOSX_BUNDLE ${MLG_APP_SOURCES})
    set_target_properties(mlg_app PROPERTIES
        OUTPUT_NAME Melange
        MACOSX_BUNDLE_GUI_IDENTIFIER io.github.niliodev.melange
        MACOSX_BUNDLE_BUNDLE_NAME Melange
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION})
endif()

target_link_libraries(mlg_app PRIVATE melange::core SDL3::SDL3)
mlg_set_warnings(mlg_app)
