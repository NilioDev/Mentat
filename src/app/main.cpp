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
#include "mentat/engine/pad_buttons.hpp"
#include "mentat/engine/platform.hpp"
#include "mentat/engine/play.hpp"
#include "mentat/engine/pointer_jump.hpp"
#include "mentat/game_files.hpp"
#include "mentat/version.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
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
    // The gamepad's pointer: used instead of the mouse's from the moment
    // the gamepad moves it or clicks, until the mouse moves again.
    bool padPointer = false;
    float padX = 0;
    float padY = 0;
    std::uint8_t padButtons = 0; // as the mouse's: bit 0 left, bit 1 right
    mentat::PadButtons buttons;  // what each button does
    Uint64 lastFrameNs = 0;

    // Copying the game files from the player's CD image, when they are not
    // found: see setupEvent.
    std::thread copier;
    std::atomic<std::uint64_t> copied{0};
    std::atomic<std::uint64_t> toCopy{0};
    std::atomic<bool> copying{false};
    std::atomic<bool> copyDone{false};
    std::string copyError; // set before copyDone
    std::atomic<bool> dialogOpen{false};
    std::mutex pickedLock; // the file dialog answers on its own thread
    std::optional<std::string> picked;
    int argc = 0;
    char** argv = nullptr;

    ~App() {
        if (copier.joinable()) {
            copier.join();
        }
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
    // Next to the AppImage file: the program itself runs from a read-only
    // mount.
    if (const char* appImage = SDL_getenv("APPIMAGE")) {
        folders.emplace_back(std::filesystem::path(appImage).parent_path() / "gamedata");
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

// Where game files copied from the player's CD go.
std::filesystem::path copiedGameFolder() {
    std::filesystem::path folder = "gamedata";
    if (char* pref = SDL_GetPrefPath("NilioDev", "Mentat")) {
        folder = std::filesystem::path(pref) / "gamedata";
        SDL_free(pref);
    }
    return folder;
}

bool isFile(const char* path) {
    std::error_code ec;
    return std::filesystem::is_regular_file(path, ec);
}

void openGameFiles(App& app, int argc, char** argv) {
    app.problem.clear();
    app.archive.reset();
    app.program.reset();
    // A CD image given to start with is copied first.
    const bool imageGiven = argc > 1 && isFile(argv[1]);
    const auto files = mentat::findGameFiles(gameFolders(imageGiven ? 1 : argc, argv));
    if (!files) {
        app.problem = "DUNE.DAT is not found.";
        return;
    }
    if (files->program.empty()) {
        app.problem = "DNCDPRG.EXE is not found next to DUNE.DAT.";
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
        // Both channels go out as 16-bit stereo: the digital one's sides
        // have their own volumes.
        const SDL_AudioSpec spec{SDL_AUDIO_S16, 2, music ? mentat::Opl::kRate : 11025};
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
        while (runIntro(game, game.language) == IntroEnd::Finished) {
        }
        runCredits(game);
        runStory(game, game.language);
    } while (mentat::runGame(game, game.language) == GameEnd::Won);
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
                const SDL_AudioSpec spec{SDL_AUDIO_S16, 2, command.rate};
                SDL_SetAudioStreamFormat(stream, &spec, nullptr);
                app.streamRates[channel] = command.rate;
            }
            const double left = mentat::levelGain(command.levels >> 4);
            const double right = mentat::levelGain(command.levels & 0x0F);
            std::vector<Sint16> frames(command.samples.size() * 2);
            for (std::size_t i = 0; i < command.samples.size(); ++i) {
                const double v = (static_cast<int>(command.samples[i]) - 128) * 256.0;
                frames[2 * i] = static_cast<Sint16>(v * left);
                frames[2 * i + 1] = static_cast<Sint16>(v * right);
            }
            SDL_PutAudioStreamData(stream, frames.data(), static_cast<int>(frames.size() * 2));
        }
    }
    app.platform.sound.clear();
}

// --- The gamepad (see docs/gamepad.md)

// The pointer the gamepad moves starts where the mouse's is.
void takePadPointer(App& app) {
    if (!app.padPointer) {
        app.padPointer = true;
        app.padX = static_cast<float>(app.platform.mouseX);
        app.padY = static_cast<float>(app.platform.mouseY);
    }
}

// The left stick moves the pointer freely, faster the further it is pushed.
void movePadPointer(App& app, float seconds) {
    constexpr float kDeadZone = 8000.0f;
    constexpr float kFullSpeed = 220.0f; // screen pixels a second
    int count = 0;
    SDL_JoystickID* ids = SDL_GetGamepads(&count);
    float ax = 0;
    float ay = 0;
    for (int i = 0; i < count; ++i) {
        SDL_Gamepad* pad = SDL_GetGamepadFromID(ids[i]);
        if (pad == nullptr) {
            continue;
        }
        const float x = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTX);
        const float y = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTY);
        if (std::fabs(x) > kDeadZone || std::fabs(y) > kDeadZone) {
            ax = x / 32767.0f;
            ay = y / 32767.0f;
        }
    }
    SDL_free(ids);
    if (ax == 0 && ay == 0) {
        return;
    }
    takePadPointer(app);
    const auto curve = [](float v) { return v * std::fabs(v); }; // finer near the middle
    app.padX = std::clamp(app.padX + curve(ax) * kFullSpeed * seconds, 0.0f, kScreenW - 1.0f);
    app.padY = std::clamp(app.padY + curve(ay) * kFullSpeed * seconds, 0.0f, kScreenH - 1.0f);
}

// The direction pad jumps to the next thing that can be clicked that way.
void jumpPadPointer(App& app, int dx, int dy) {
    takePadPointer(app);
    const auto to = mentat::jumpTarget(app.platform.pointerTargets, static_cast<int>(app.padX), static_cast<int>(app.padY),
                                       dx, dy);
    if (to) {
        app.padX = static_cast<float>(to->x);
        app.padY = static_cast<float>(to->y);
    }
}

void updateInput(App& app) {
    using namespace mentat;
    auto& p = app.platform;
    p.ticks = static_cast<std::uint32_t>(static_cast<double>(SDL_GetTicksNS() - app.startNs) * kTicksPerSecond / 1e9);
    float x = 0;
    float y = 0;
    const SDL_MouseButtonFlags buttons = SDL_GetMouseState(&x, &y);
    SDL_RenderCoordinatesFromWindow(app.renderer, x, y, &x, &y);
    const Uint64 now = SDL_GetTicksNS();
    const float seconds = app.lastFrameNs == 0 ? 0.0f : static_cast<float>(now - app.lastFrameNs) / 1e9f;
    app.lastFrameNs = now;
    movePadPointer(app, seconds);
    if (app.padPointer) {
        p.mouseX = std::clamp(static_cast<int>(app.padX), 0, kScreenW - 1);
        p.mouseY = std::clamp(static_cast<int>(app.padY), 0, kScreenH - 1);
    } else {
        p.mouseX = std::clamp(static_cast<int>(std::floor(x)), 0, kScreenW - 1);
        p.mouseY = std::clamp(static_cast<int>(std::floor(y)), 0, kScreenH - 1);
    }
    p.mouseButtons = static_cast<std::uint8_t>(((buttons & SDL_BUTTON_LMASK) != 0 ? 1 : 0) |
                                               ((buttons & SDL_BUTTON_RMASK) != 0 ? 2 : 0) | app.padButtons);
    for (int i = 0; i < kChannels; ++i) {
        SDL_AudioStream* stream = app.streams[static_cast<std::size_t>(i)];
        const int rate = app.streamRates[static_cast<std::size_t>(i)];
        const double bytesPerSecond = rate * 4.0; // 16-bit stereo frames
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

static_assert(SDL_GAMEPAD_BUTTON_SOUTH == 0 && SDL_GAMEPAD_BUTTON_BACK == 4 && SDL_GAMEPAD_BUTTON_DPAD_UP == 11 &&
                  SDL_GAMEPAD_BUTTON_DPAD_RIGHT == 14 && SDL_GAMEPAD_BUTTON_COUNT <= mentat::PadButtons::kButtons,
              "PadButtons counts the buttons in SDL's order");

// The player's own choice of what the gamepad's buttons do, from
// gamepad.txt beside the saved games (see docs/gamepad.md).
void readPadButtons(App& app) {
    char* pref = SDL_GetPrefPath("NilioDev", "Mentat");
    if (pref == nullptr) {
        return;
    }
    const std::filesystem::path file = std::filesystem::path(pref) / "gamepad.txt";
    SDL_free(pref);
    std::ifstream in(file, std::ios::binary);
    if (!in) {
        return;
    }
    const std::string text((std::istreambuf_iterator<char>(in)), {});
    if (const int bad = app.buttons.read(text); bad > 0) {
        SDL_Log("gamepad.txt: %d line(s) not understood", bad);
    }
}

void startGame(App& app) {
    App* raw = &app;
    app.game = std::make_unique<mentat::Fiber>([raw] { runGame(*raw); });
    app.platform.fiber = app.game.get();
    app.startNs = SDL_GetTicksNS();
}

// Copies the game files from a CD image or folder the player gave, on a
// thread of its own: the window keeps showing how far it is.
void startCopy(App& app, const std::string& source) {
    if (app.copying) {
        return;
    }
    if (app.copier.joinable()) {
        app.copier.join();
    }
    app.copyError.clear();
    app.copied = 0;
    app.toCopy = 0;
    app.copyDone = false;
    app.copying = true;
    App* raw = &app;
    app.copier = std::thread([raw, source] {
        try {
            mentat::importGameFiles(source, copiedGameFolder(), [raw](std::uint64_t done, std::uint64_t total) {
                raw->copied = done;
                raw->toCopy = total;
            });
        } catch (const std::exception& e) {
            raw->copyError = e.what();
        }
        raw->copyDone = true;
    });
}

void SDLCALL picked(void* userdata, const char* const* files, int) {
    auto* app = static_cast<App*>(userdata);
    const std::lock_guard lock(app->pickedLock);
    app->dialogOpen = false;
    if (files != nullptr && files[0] != nullptr) {
        app->picked = files[0];
    }
}

void pickImage(App& app) {
    if (app.copying || app.dialogOpen) {
        return;
    }
    static const SDL_DialogFileFilter kFilters[] = {
        {"CD image", "iso;bin;img"},
        {"All files", "*"},
    };
    app.dialogOpen = true;
    SDL_ShowOpenFileDialog(picked, &app, app.window, kFilters, 2, nullptr, false);
}

// The events while there is no game yet: the player gives the CD image by
// dropping it on the window, or picks it with a click, Enter or a gamepad
// button.
SDL_AppResult setupEvent(App& app, const SDL_Event& event) {
    switch (event.type) {
    case SDL_EVENT_QUIT:
        return SDL_APP_SUCCESS;
    case SDL_EVENT_KEY_DOWN:
        if (event.key.key == SDLK_ESCAPE) {
            return SDL_APP_SUCCESS;
        }
        if (event.key.key == SDLK_RETURN || event.key.key == SDLK_SPACE || event.key.key == SDLK_KP_ENTER) {
            pickImage(app);
        }
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        pickImage(app);
        break;
    case SDL_EVENT_GAMEPAD_ADDED:
        SDL_OpenGamepad(event.gdevice.which);
        break;
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        if (app.buttons.action(event.gbutton.button) == mentat::PadAction::Quit) {
            return SDL_APP_SUCCESS;
        }
        pickImage(app);
        break;
    case SDL_EVENT_DROP_FILE:
        if (event.drop.data != nullptr) {
            startCopy(app, event.drop.data);
        }
        break;
    default:
        break;
    }
    return SDL_APP_CONTINUE;
}

// Called each frame while there is no game: starts a copy the dialog
// picked, and the game once a copy is done.
void checkCopy(App& app) {
    std::optional<std::string> source;
    {
        const std::lock_guard lock(app.pickedLock);
        source.swap(app.picked);
    }
    if (source) {
        startCopy(app, *source);
    }
    if (!app.copyDone) {
        return;
    }
    app.copier.join();
    app.copyDone = false;
    app.copying = false;
    if (!app.copyError.empty()) {
        SDL_Log("copying the game files: %s", app.copyError.c_str());
        return;
    }
    openGameFiles(app, app.argc, app.argv);
    if (app.problem.empty()) {
        startGame(app);
    } else {
        app.copyError = app.problem;
    }
}

// Writes text in SDL's own small font, broken into lines that fit the
// screen; returns the y under it.
float writeText(SDL_Renderer* renderer, float y, const std::string& text) {
    constexpr std::size_t kColumns = (kScreenW - 16) / SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE;
    std::size_t start = 0;
    while (start < text.size()) {
        std::size_t end = std::min(text.size(), start + kColumns);
        if (end < text.size()) {
            const std::size_t space = text.rfind(' ', end);
            if (space != std::string::npos && space > start) {
                end = space;
            }
        }
        const std::string line = text.substr(start, end - start);
        SDL_RenderDebugText(renderer, 8, y, line.c_str());
        y += SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE + 3;
        start = end;
        while (start < text.size() && text[start] == ' ') {
            ++start;
        }
    }
    return y;
}

// The screen while there is no game: what is missing, and how to give it.
void drawSetup(App& app) {
    SDL_Renderer* r = app.renderer;
    SDL_SetRenderDrawColor(r, 0xE8, 0xC0, 0x70, 255);
    float y = writeText(r, 12, std::string(mentat::kProjectName));
    y += 8;
    SDL_SetRenderDrawColor(r, 0xF0, 0xE6, 0xD2, 255);
    if (app.copying) {
        const std::uint64_t total = app.toCopy;
        const std::uint64_t done = app.copied;
        const int percent = total == 0 ? 0 : static_cast<int>(done * 100 / total);
        y = writeText(r, y, "Copying the game files from your CD... " + std::to_string(percent) + "%");
        y += 6;
        SDL_SetRenderDrawColor(r, 0x50, 0x40, 0x30, 255);
        const SDL_FRect back{8, y, kScreenW - 16.0f, 8};
        SDL_RenderFillRect(r, &back);
        SDL_SetRenderDrawColor(r, 0xE8, 0xC0, 0x70, 255);
        const SDL_FRect bar{8, y, (kScreenW - 16.0f) * static_cast<float>(percent) / 100.0f, 8};
        SDL_RenderFillRect(r, &bar);
        return;
    }
    if (!app.copyError.empty()) {
        SDL_SetRenderDrawColor(r, 0xF0, 0x70, 0x60, 255);
        y = writeText(r, y, "That did not work: " + app.copyError);
        SDL_SetRenderDrawColor(r, 0xF0, 0xE6, 0xD2, 255);
    } else {
        y = writeText(r, y, app.problem);
    }
    y += 6;
    y = writeText(r, y, "Mentat needs DUNE.DAT and DNCDPRG.EXE from your game CD.");
    y += 6;
    y = writeText(r, y, "Drop your CD image (.iso or .bin) or the CD itself on this window, or click, press Enter or "
                        "a gamepad button to pick it. Mentat copies the two files and starts.");
    y += 6;
    SDL_SetRenderDrawColor(r, 0xA0, 0x98, 0x88, 255);
    writeText(r, y, "Esc quits.");
}

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
    readPadButtons(*app);

    app->argc = argc;
    app->argv = argv;
    openGameFiles(*app, argc, argv);
    if (app->problem.empty()) {
        startGame(*app);
    } else {
        SDL_Log("%s", app->problem.c_str());
        if (argc > 1 && isFile(argv[1])) {
            startCopy(*app, argv[1]);
        }
    }
    *state = app.release();
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void* state, SDL_Event* event) {
    auto* app = static_cast<App*>(state);
    if (app->game == nullptr) {
        return setupEvent(*app, *event);
    }
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
    case SDL_EVENT_MOUSE_MOTION:
        app->padPointer = false;
        return SDL_APP_CONTINUE;
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        switch (app->buttons.action(event->gbutton.button)) {
        case mentat::PadAction::Quit:
            return SDL_APP_SUCCESS;
        case mentat::PadAction::Up: jumpPadPointer(*app, 0, -1); break;
        case mentat::PadAction::Down: jumpPadPointer(*app, 0, 1); break;
        case mentat::PadAction::Left: jumpPadPointer(*app, -1, 0); break;
        case mentat::PadAction::Right: jumpPadPointer(*app, 1, 0); break;
        case mentat::PadAction::Click:
            takePadPointer(*app);
            app->padButtons |= 1;
            break;
        case mentat::PadAction::RightClick:
            takePadPointer(*app);
            app->padButtons |= 2;
            break;
        case mentat::PadAction::Key:
            app->platform.keys.push_back(mentat::KeyPress{0x39, ' '});
            break;
        case mentat::PadAction::None:
            break;
        }
        return SDL_APP_CONTINUE;
    case SDL_EVENT_GAMEPAD_BUTTON_UP:
        switch (app->buttons.action(event->gbutton.button)) {
        case mentat::PadAction::Click: app->padButtons &= static_cast<std::uint8_t>(~1u); break;
        case mentat::PadAction::RightClick: app->padButtons &= static_cast<std::uint8_t>(~2u); break;
        default: break;
        }
        return SDL_APP_CONTINUE;
    default:
        return SDL_APP_CONTINUE;
    }
}

SDL_AppResult SDL_AppIterate(void* state) {
    auto* app = static_cast<App*>(state);
    if (app->game == nullptr) {
        checkCopy(*app);
    }
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
        drawSetup(*app);
    }
    SDL_RenderPresent(app->renderer);
    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* state, SDL_AppResult) {
    delete static_cast<App*>(state);
}
