// SPDX-License-Identifier: GPL-3.0-or-later
//
// The Mentat app. For now it opens a window, looks for the player's
// DUNE.DAT, opens it, and shows a 320×200 picture scaled sharply to the
// window. Esc, the window's close button or B / Back on a gamepad close it.
#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "mentat/data/dat_archive.hpp"
#include "mentat/data/reader.hpp"
#include "mentat/game_files.hpp"
#include "mentat/version.hpp"

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace {

constexpr int kScreenW = 320;
constexpr int kScreenH = 200;

struct App {
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    std::optional<mentat::DatArchive> archive;
    std::string problem;
};

std::vector<std::filesystem::path> gameFolders(int argc, char** argv) {
    std::vector<std::filesystem::path> folders;
    if (argc > 1) {
        folders.emplace_back(argv[1]);
    }
    if (const char* base = SDL_GetBasePath()) {
        folders.emplace_back(std::filesystem::path(base) / "gamedata");
        folders.emplace_back(base);
    }
    folders.emplace_back("gamedata");
    if (char* pref = SDL_GetPrefPath("NilioDev", "Mentat")) {
        folders.emplace_back(std::filesystem::path(pref) / "gamedata");
        SDL_free(pref);
    }
    return folders;
}

} // namespace

SDL_AppResult SDL_AppInit(void** state, int argc, char** argv) {
    auto app = std::make_unique<App>();
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    const std::string title(mentat::kProjectName);
    if (!SDL_CreateWindowAndRenderer(title.c_str(), kScreenW * 4, kScreenH * 4, SDL_WINDOW_RESIZABLE,
                                     &app->window, &app->renderer)) {
        SDL_Log("cannot open a window: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    SDL_SetRenderLogicalPresentation(app->renderer, kScreenW, kScreenH, SDL_LOGICAL_PRESENTATION_INTEGER_SCALE);

    if (const auto files = mentat::findGameFiles(gameFolders(argc, argv))) {
        try {
            app->archive = mentat::DatArchive::open(files->dat);
            SDL_Log("opened %s: %zu files", files->dat.string().c_str(),
                    app->archive->directory().entries().size());
        } catch (const mentat::FormatError& e) {
            app->problem = e.what();
        }
    } else {
        app->problem = "DUNE.DAT not found: put it in a folder named gamedata next to the program";
    }
    if (!app->problem.empty()) {
        SDL_Log("%s", app->problem.c_str());
    }
    *state = app.release();
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void*, SDL_Event* event) {
    switch (event->type) {
    case SDL_EVENT_QUIT:
        return SDL_APP_SUCCESS;
    case SDL_EVENT_KEY_DOWN:
        return event->key.key == SDLK_ESCAPE ? SDL_APP_SUCCESS : SDL_APP_CONTINUE;
    case SDL_EVENT_GAMEPAD_ADDED:
        SDL_OpenGamepad(event->gdevice.which);
        return SDL_APP_CONTINUE;
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        return event->gbutton.button == SDL_GAMEPAD_BUTTON_EAST || event->gbutton.button == SDL_GAMEPAD_BUTTON_BACK
                   ? SDL_APP_SUCCESS
                   : SDL_APP_CONTINUE;
    default:
        return SDL_APP_CONTINUE;
    }
}

SDL_AppResult SDL_AppIterate(void* state) {
    auto* app = static_cast<App*>(state);
    SDL_SetRenderDrawColor(app->renderer, 0, 0, 0, 255);
    SDL_RenderClear(app->renderer);
    // Our own colours: sand when the files opened, red when something is missing.
    if (app->archive) {
        SDL_SetRenderDrawColor(app->renderer, 0xD8, 0xA8, 0x60, 255);
    } else {
        SDL_SetRenderDrawColor(app->renderer, 0xA0, 0x30, 0x30, 255);
    }
    const SDL_FRect bar{0, kScreenH - 8.0f, kScreenW, 8.0f};
    SDL_RenderFillRect(app->renderer, &bar);
    SDL_RenderPresent(app->renderer);
    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* state, SDL_AppResult) {
    delete static_cast<App*>(state);
}
