// SPDX-License-Identifier: GPL-3.0-or-later
//
// The Mentat app: the SDL3 platform layer. It opens a window and the sound
// device, finds the player's game files, and runs the game on its own thread
// (see mentat/engine/fiber.hpp), one step per frame. Each frame it hands the
// game the clock, mouse and keys, then shows the game's screen scaled sharply
// to the window and plays the sound the game queued.
#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "mentat/audio/music.hpp"
#include "mentat/audio/opl.hpp"
#include "mentat/data/dat_archive.hpp"
#include "mentat/data/program_file.hpp"
#include "mentat/data/program_tables.hpp"
#include "mentat/data/reader.hpp"
#include "mentat/engine/display.hpp"
#include "mentat/engine/fiber.hpp"
#include "mentat/engine/game.hpp"
#include "mentat/engine/intro.hpp"
#include "mentat/engine/platform.hpp"
#include "mentat/engine/play.hpp"
#include "mentat/game_files.hpp"
#include "mentat/version.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace {

constexpr int kScreenW = 320;
constexpr int kScreenH = 200;
constexpr int kChannels = static_cast<int>(mentat::SoundChannel::Count);

struct App {
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_Texture* texture = nullptr;
    SDL_AudioDeviceID audio = 0;
    std::array<SDL_AudioStream*, kChannels> streams{};
    std::array<int, kChannels> streamRates{};
    std::optional<mentat::DatArchive> archive;
    std::optional<mentat::ProgramFile> program;
    std::string problem;
    mentat::Platform platform;
    std::unique_ptr<mentat::Fiber> game;
    Uint64 startNs = 0;
    std::vector<Uint32> rgba = std::vector<Uint32>(static_cast<std::size_t>(kScreenW) * kScreenH);
    mentat::Surface shown; // the screen with the mouse pointer over it
    bool systemCursor = true;

    ~App() {
        game.reset(); // stop the game thread before what it uses goes away
        for (auto* stream : streams) {
            if (stream != nullptr) {
                SDL_DestroyAudioStream(stream);
            }
        }
        if (audio != 0) {
            SDL_CloseAudioDevice(audio);
        }
    }
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

void openGameFiles(App& app, int argc, char** argv) {
    const auto files = mentat::findGameFiles(gameFolders(argc, argv));
    if (!files) {
        app.problem = "DUNE.DAT not found: put it in a folder named gamedata next to the program";
        return;
    }
    if (files->program.empty()) {
        app.problem = "DNCDPRG.EXE not found: copy it from the CD next to DUNE.DAT";
        return;
    }
    try {
        app.archive = mentat::DatArchive::open(files->dat);
        app.program = mentat::ProgramFile::open(files->program);
    } catch (const mentat::FormatError& e) {
        app.problem = e.what();
    }
}

void openAudio(App& app) {
    app.audio = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
    if (app.audio == 0) {
        SDL_Log("no sound: %s", SDL_GetError());
        return;
    }
    for (int i = 0; i < kChannels; ++i) {
        const bool music = i == static_cast<int>(mentat::SoundChannel::Music);
        const SDL_AudioSpec spec = music ? SDL_AudioSpec{SDL_AUDIO_S16, 2, mentat::Opl::kRate}
                                         : SDL_AudioSpec{SDL_AUDIO_U8, 1, 11025};
        app.streams[static_cast<std::size_t>(i)] = SDL_CreateAudioStream(&spec, nullptr);
        app.streamRates[static_cast<std::size_t>(i)] = spec.freq;
        SDL_BindAudioStream(app.audio, app.streams[static_cast<std::size_t>(i)]);
    }
}

// The game: for now the opening, which starts over until a key is pressed,
// like the original's.
void runGame(App& app) {
    using namespace mentat;
    // The music as on the AdLib Gold when the game files have it (the CD
    // version does).
    MusicSystem music(MusicTables::load(*app.archive, MusicCard::AdLibGold));
    app.platform.music = &music;
    // Saved games go beside the player's own settings.
    if (char* pref = SDL_GetPrefPath("NilioDev", "Mentat")) {
        app.platform.saveFolder = std::filesystem::path(pref) / "saves";
        SDL_free(pref);
    }
    Game game(app.platform, *app.archive, *app.program, DisplayTables::fromDriver(app.archive->load("DNVGA.HSQ")),
              &music);
    // After the end of the story the whole program starts over, from the
    // opening.
    do {
        while (runIntro(game, kEnglish) == IntroEnd::Finished) {
        }
        runCredits(game);
        runStory(game, kEnglish);
    } while (mentat::runGame(game, kEnglish) == GameEnd::Won);
    app.platform.music = nullptr;
}

void playQueuedSound(App& app) {
    for (auto& command : app.platform.sound) {
        const auto channel = static_cast<std::size_t>(command.channel);
        SDL_AudioStream* stream = app.streams[channel];
        if (stream == nullptr) {
            continue;
        }
        if (command.stop) {
            SDL_ClearAudioStream(stream);
        }
        if (command.channel == mentat::SoundChannel::Music) {
            // The game makes music as its clock runs, so the stream is kept a
            // little ahead to cover uneven frames.
            constexpr int kFrame = 4; // bytes of one stereo frame
            constexpr int kAhead = mentat::Opl::kRate / 20 * kFrame;
            const int queued = SDL_GetAudioStreamQueued(stream);
            if (queued < kAhead / 4) {
                const std::vector<Sint16> silence(static_cast<std::size_t>((kAhead / 2 - queued) / kFrame * 2), 0);
                SDL_PutAudioStreamData(stream, silence.data(), static_cast<int>(silence.size() * 2));
            } else if (queued > kAhead * 6) {
                SDL_ClearAudioStream(stream);
            }
            SDL_PutAudioStreamData(stream, command.wide.data(), static_cast<int>(command.wide.size() * 2));
            continue;
        }
        if (command.rate > 0 && !command.samples.empty()) {
            if (command.rate != app.streamRates[channel]) {
                const SDL_AudioSpec spec{SDL_AUDIO_U8, 1, command.rate};
                SDL_SetAudioStreamFormat(stream, &spec, nullptr);
                app.streamRates[channel] = command.rate;
            }
            SDL_PutAudioStreamData(stream, command.samples.data(), static_cast<int>(command.samples.size()));
        }
    }
    app.platform.sound.clear();
}

void updateInput(App& app) {
    using namespace mentat;
    auto& p = app.platform;
    p.ticks = static_cast<std::uint32_t>(static_cast<double>(SDL_GetTicksNS() - app.startNs) * kTicksPerSecond / 1e9);
    float x = 0;
    float y = 0;
    const SDL_MouseButtonFlags buttons = SDL_GetMouseState(&x, &y);
    SDL_RenderCoordinatesFromWindow(app.renderer, x, y, &x, &y);
    p.mouseX = std::clamp(static_cast<int>(std::floor(x)), 0, kScreenW - 1);
    p.mouseY = std::clamp(static_cast<int>(std::floor(y)), 0, kScreenH - 1);
    p.mouseButtons = static_cast<std::uint8_t>(((buttons & SDL_BUTTON_LMASK) != 0 ? 1 : 0) |
                                               ((buttons & SDL_BUTTON_RMASK) != 0 ? 2 : 0));
    for (int i = 0; i < kChannels; ++i) {
        SDL_AudioStream* stream = app.streams[static_cast<std::size_t>(i)];
        const int rate = app.streamRates[static_cast<std::size_t>(i)];
        const double bytesPerSecond = rate * (i == static_cast<int>(SoundChannel::Music) ? 4.0 : 1.0);
        p.soundQueued[i] = stream != nullptr && rate > 0 ? SDL_GetAudioStreamQueued(stream) / bytesPerSecond : 0.0;
    }
}

void drawScreen(App& app) {
    mentat::Surface& screen = app.shown;
    screen = app.platform.screen;
    if (app.platform.cursorShown) {
        app.platform.cursor.draw(screen, app.platform.mouseX, app.platform.mouseY);
    }
    // The game draws its own pointer; the system's is hidden while it shows.
    if (app.platform.cursorShown == app.systemCursor) {
        app.systemCursor = !app.platform.cursorShown;
        if (app.systemCursor) {
            SDL_ShowCursor();
        } else {
            SDL_HideCursor();
        }
    }
    const auto& palette = app.platform.palette;
    std::array<Uint32, 256> colours{};
    for (std::size_t i = 0; i < colours.size(); ++i) {
        const auto& c = palette[i];
        colours[i] = 0xFF000000u | (static_cast<Uint32>(mentat::Palette::to8Bit(c.r)) << 16) |
                     (static_cast<Uint32>(mentat::Palette::to8Bit(c.g)) << 8) | mentat::Palette::to8Bit(c.b);
    }
    for (std::size_t i = 0; i < app.rgba.size(); ++i) {
        app.rgba[i] = colours[screen.data()[i]];
    }
    SDL_UpdateTexture(app.texture, nullptr, app.rgba.data(), kScreenW * 4);
    SDL_RenderTexture(app.renderer, app.texture, nullptr, nullptr);
}

} // namespace

SDL_AppResult SDL_AppInit(void** state, int argc, char** argv) {
    auto app = std::make_unique<App>();
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    const std::string title(mentat::kProjectName);
    if (!SDL_CreateWindowAndRenderer(title.c_str(), kScreenW * 4, kScreenH * 4, SDL_WINDOW_RESIZABLE,
                                     &app->window, &app->renderer)) {
        SDL_Log("cannot open a window: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    SDL_SetRenderLogicalPresentation(app->renderer, kScreenW, kScreenH, SDL_LOGICAL_PRESENTATION_LETTERBOX);
    SDL_SetRenderVSync(app->renderer, 1);
    app->texture =
        SDL_CreateTexture(app->renderer, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STREAMING, kScreenW, kScreenH);
    SDL_SetTextureScaleMode(app->texture, SDL_SCALEMODE_NEAREST);
    openAudio(*app);

    openGameFiles(*app, argc, argv);
    if (app->problem.empty()) {
        App* raw = app.get();
        app->game = std::make_unique<mentat::Fiber>([raw] { runGame(*raw); });
        app->platform.fiber = app->game.get();
    } else {
        SDL_Log("%s", app->problem.c_str());
    }
    app->startNs = SDL_GetTicksNS();
    *state = app.release();
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void* state, SDL_Event* event) {
    auto* app = static_cast<App*>(state);
    switch (event->type) {
    case SDL_EVENT_QUIT:
        return SDL_APP_SUCCESS;
    case SDL_EVENT_KEY_DOWN:
        if (event->key.key == SDLK_ESCAPE && app->game == nullptr) {
            return SDL_APP_SUCCESS;
        }
        app->platform.keys.push_back(mentat::KeyPress{static_cast<std::uint8_t>(event->key.raw), 0});
        return SDL_APP_CONTINUE;
    case SDL_EVENT_GAMEPAD_ADDED:
        SDL_OpenGamepad(event->gdevice.which);
        return SDL_APP_CONTINUE;
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        if (event->gbutton.button == SDL_GAMEPAD_BUTTON_BACK) {
            return SDL_APP_SUCCESS;
        }
        app->platform.keys.push_back(mentat::KeyPress{0x39, ' '});
        return SDL_APP_CONTINUE;
    default:
        return SDL_APP_CONTINUE;
    }
}

SDL_AppResult SDL_AppIterate(void* state) {
    auto* app = static_cast<App*>(state);
    if (app->game != nullptr) {
        updateInput(*app);
        try {
            app->game->resume();
        } catch (const std::exception& e) {
            SDL_Log("the game stopped: %s", e.what());
            return SDL_APP_FAILURE;
        }
        playQueuedSound(*app);
        if (app->game->finished()) {
            return SDL_APP_SUCCESS;
        }
    }
    SDL_SetRenderDrawColor(app->renderer, 0, 0, 0, 255);
    SDL_RenderClear(app->renderer);
    if (app->game != nullptr) {
        drawScreen(*app);
    } else {
        // Our own colour: red when something is missing.
        SDL_SetRenderDrawColor(app->renderer, 0xA0, 0x30, 0x30, 255);
        const SDL_FRect bar{0, kScreenH - 8.0f, kScreenW, 8.0f};
        SDL_RenderFillRect(app->renderer, &bar);
    }
    SDL_RenderPresent(app->renderer);
    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* state, SDL_AppResult) {
    delete static_cast<App*>(state);
}
