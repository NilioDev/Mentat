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
#ifdef MENTAT_WITH_MT32
#include "mentat/audio/mt32_music.hpp"
#endif
#include "mentat/data/dat_archive.hpp"
#include "mentat/data/program_file.hpp"
#include "mentat/data/program_tables.hpp"
#include "mentat/data/reader.hpp"
#include "mentat/engine/display.hpp"
#include "mentat/engine/fiber.hpp"
#include "mentat/engine/game.hpp"
#include "mentat/engine/intro.hpp"
#include "mentat/engine/options.hpp"
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
#include <cstring>
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
    // found: see addFiles.
    std::thread copier;
    std::atomic<std::uint64_t> copied{0};
    std::atomic<std::uint64_t> toCopy{0};
    std::atomic<bool> copying{false};
    std::atomic<bool> copyDone{false};
    std::string copyError; // set before copyDone
    std::atomic<bool> dialogOpen{false};
    std::mutex pickedLock; // the file dialog answers on its own thread
    std::vector<std::string> picked; // files the player dropped or picked
    bool gameFound = false;          // the game CD's files
    bool romsFound = false;          // the MT-32 ROMs
    bool filesOpen = false;          // the game files screen, over the menu
    int filesItem = 0;               // its button with the focus
    std::string filesNote;           // what adding files came to
    bool noteIsError = false;
    int argc = 0;
    char** argv = nullptr;

    // Mentat's own options and the menu that sets them (see drawMenu).
    mentat::Options options;
    std::unique_ptr<mentat::MusicSystem> music;
    mentat::Game* liveGame = nullptr; // while the game runs
    bool menuOpen = false;
    int menuItem = 0;
    Uint64 menuOpenedNs = 0;
    std::string menuNote;

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

std::filesystem::path prefFile(const char* name) {
    std::filesystem::path path = name;
    if (char* pref = SDL_GetPrefPath("NilioDev", "Mentat")) {
        path = std::filesystem::path(pref) / name;
        SDL_free(pref);
    }
    return path;
}

void readOptions(App& app) {
    std::ifstream in(prefFile("options.txt"), std::ios::binary);
    if (in) {
        const std::string text((std::istreambuf_iterator<char>(in)), {});
        app.options = mentat::Options::read(text);
    }
}

void writeOptions(const App& app) {
    std::ofstream out(prefFile("options.txt"), std::ios::binary | std::ios::trunc);
    out << app.options.write();
    if (!out) {
        SDL_Log("cannot save the options");
    }
}

// The music player for a choice; null when it cannot play here.
std::unique_ptr<mentat::MusicSystem> makeMusic(App& app, mentat::MusicChoice choice) {
    using namespace mentat;
    try {
        switch (choice) {
        case MusicChoice::AdLib:
            return std::make_unique<FmMusic>(MusicTables::load(*app.archive, MusicCard::SoundBlasterPro));
        case MusicChoice::AdLibGold:
            return std::make_unique<FmMusic>(MusicTables::load(*app.archive, MusicCard::AdLibGold));
        case MusicChoice::Mt32: {
#ifdef MENTAT_WITH_MT32
            const auto roms = findMt32Roms(gameFolders(app.argc, app.argv));
            if (!roms) {
                app.menuNote = "Add the MT-32 ROMs under Game files.";
                return nullptr;
            }
            return std::make_unique<Mt32Music>(roms->control, roms->pcm);
#else
            app.menuNote = "This Mentat is built without the MT-32.";
            return nullptr;
#endif
        }
        }
    } catch (const std::exception& e) {
        SDL_Log("no music player: %s", e.what());
        app.menuNote = e.what();
    }
    return nullptr;
}

// The game: the opening, which starts over until a key is pressed, like the
// original's, then the story and play.
void runGame(App& app) {
    using namespace mentat;
    app.music = makeMusic(app, app.options.music);
    if (app.music == nullptr) {
        app.music = makeMusic(app, MusicChoice::AdLibGold);
    }
    app.platform.music = app.music.get();
    // Saved games go beside the player's own settings.
    if (char* pref = SDL_GetPrefPath("NilioDev", "Mentat")) {
        app.platform.saveFolder = std::filesystem::path(pref) / "saves";
        SDL_free(pref);
    }
    Game game(app.platform, *app.archive, *app.program, DisplayTables::fromDriver(app.archive->load("DNVGA.HSQ")),
              app.music.get());
    app.liveGame = &game;
    // After the end of the story the whole program starts over, from the
    // opening.
    do {
        while (runIntro(game, game.language) == IntroEnd::Finished) {
        }
        runCredits(game);
        runStory(game, game.language);
    } while (mentat::runGame(game, game.language) == GameEnd::Won);
    app.liveGame = nullptr;
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
            if (command.rate != app.streamRates[channel]) {
                // Another player, at its own rate.
                const SDL_AudioSpec spec{SDL_AUDIO_S16, 2, command.rate};
                SDL_ClearAudioStream(stream);
                SDL_SetAudioStreamFormat(stream, &spec, nullptr);
                app.streamRates[channel] = command.rate;
            }
            const int kAhead = command.rate / 20 * kFrame;
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
    for (int i = 0; files != nullptr && files[i] != nullptr; ++i) {
        app->picked.emplace_back(files[i]);
    }
}

void pickFiles(App& app) {
    if (app.copying || app.dialogOpen) {
        return;
    }
    static const SDL_DialogFileFilter kFilters[] = {
        {"CD image or MT-32 ROM", "iso;bin;img;rom"},
        {"All files", "*"},
    };
    app.dialogOpen = true;
    SDL_ShowOpenFileDialog(picked, &app, app.window, kFilters, 2, nullptr, true);
}

// The name an optional file the player gave is kept under; empty for any
// other file.
std::string extraFileName(const std::filesystem::path& file) {
#ifdef MENTAT_WITH_MT32
    return mentat::mt32RomName(file);
#else
    (void)file;
    return {};
#endif
}

void findExtras(App& app) { app.romsFound = mentat::findMt32Roms(gameFolders(app.argc, app.argv)).has_value(); }

// Once the MT-32 ROMs are added, the MT-32 music the player chose before
// plays instead of the AdLib Gold's that stood in for it.
void retryMusic(App& app) {
    if (app.liveGame == nullptr || app.music == nullptr || app.options.music != mentat::MusicChoice::Mt32 ||
        app.music->card() == mentat::MusicCard::Mt32) {
        return;
    }
    if (auto next = makeMusic(app, mentat::MusicChoice::Mt32)) {
        app.liveGame->switchMusic(next.get());
        app.music = std::move(next);
    }
}

// Files the player dropped on the window or picked: the MT-32 ROMs (by
// their contents, whatever their names) are copied at once, and the game
// CD's files on a thread of their own. A folder can hold both.
void addFiles(App& app, const std::vector<std::string>& sources) {
    std::vector<std::filesystem::path> extras;
    std::optional<std::string> image;
    const bool needGame = app.game == nullptr && !app.gameFound;
    for (const auto& source : sources) {
        std::error_code ec;
        if (std::filesystem::is_directory(source, ec)) {
            extras.emplace_back(source);
            if (needGame && mentat::findGameFiles({source})) {
                image = source;
            }
        } else if (!extraFileName(source).empty()) {
            extras.emplace_back(source);
        } else if (needGame) {
            image = source;
        } else {
            app.filesNote = "Mentat already has the game CD's files.";
            app.noteIsError = false;
        }
    }
    if (!extras.empty()) {
        try {
            const auto names = mentat::importExtraFiles(extras, copiedGameFolder(), extraFileName);
            if (!names.empty()) {
                std::string text = "Added";
                for (std::size_t i = 0; i < names.size(); ++i) {
                    text += (i == 0 ? " " : (i + 1 == names.size() ? " and " : ", ")) + names[i];
                }
                app.filesNote = text + ".";
                app.noteIsError = false;
            } else if (!image) {
                app.filesNote = "There are no MT-32 ROMs in that folder.";
                app.noteIsError = true;
            }
        } catch (const std::exception& e) {
            app.filesNote = e.what();
            app.noteIsError = true;
        }
        findExtras(app);
        retryMusic(app);
    }
    if (image) {
        startCopy(app, *image);
    }
}

// Called each frame: takes the files the player gave, and finishes a copy
// of the game CD's files.
void checkCopy(App& app) {
    std::vector<std::string> sources;
    {
        const std::lock_guard lock(app.pickedLock);
        sources.swap(app.picked);
    }
    if (!sources.empty()) {
        addFiles(app, sources);
    }
    if (!app.copyDone) {
        return;
    }
    app.copier.join();
    app.copyDone = false;
    app.copying = false;
    if (!app.copyError.empty()) {
        SDL_Log("copying the game files: %s", app.copyError.c_str());
        app.filesNote = "That did not work: " + app.copyError;
        app.noteIsError = true;
        return;
    }
    openGameFiles(app, app.argc, app.argv);
    app.gameFound = app.problem.empty();
    if (app.gameFound) {
        // Keeps saying which ROMs were added with them.
        app.filesNote = "The game CD's files are copied." +
                        (app.filesNote.empty() || app.noteIsError ? std::string() : " " + app.filesNote);
        app.noteIsError = false;
        app.filesItem = 1; // Play
    } else {
        app.filesNote = "That did not work: " + app.problem;
        app.noteIsError = true;
    }
    findExtras(app);
}

// Writes text in SDL's own small font, broken into lines that fit the
// screen; returns the y under it.
float writeText(SDL_Renderer* renderer, float y, const std::string& text, float x = 8,
                std::size_t columns = (kScreenW - 16) / SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE) {
    std::size_t start = 0;
    while (start < text.size()) {
        std::size_t end = std::min(text.size(), start + columns);
        if (end < text.size()) {
            const std::size_t space = text.rfind(' ', end);
            if (space != std::string::npos && space > start) {
                end = space;
            }
        }
        const std::string line = text.substr(start, end - start);
        SDL_RenderDebugText(renderer, x, y, line.c_str());
        y += SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE + 3;
        start = end;
        while (start < text.size() && text[start] == ' ') {
            ++start;
        }
    }
    return y;
}

// The game files screen: what Mentat needs to play, what it can use besides,
// and the way to add both. It is the first screen while the game CD's files
// are missing, and is opened from the options menu later.
constexpr float kFilesLeft = 12;
constexpr float kFilesRight = kScreenW - 12;
constexpr SDL_FRect kFilesButtons[] = {
    {kFilesLeft, 150, 136, 15},
    {kFilesRight - 136, 150, 136, 15},
};
constexpr int kFilesButtonCount = static_cast<int>(std::size(kFilesButtons));

int filesButtonAt(float x, float y) {
    for (int i = 0; i < kFilesButtonCount; ++i) {
        const SDL_FRect& b = kFilesButtons[i];
        if (x >= b.x && x < b.x + b.w && y >= b.y && y < b.y + b.h) {
            return i;
        }
    }
    return -1;
}

bool inGame(const App& app) { return app.game != nullptr; }

bool canPlay(const App& app) { return inGame(app) || (app.gameFound && !app.copying); }

void openFiles(App& app) {
    app.filesOpen = true;
    app.filesItem = 0;
    app.filesNote.clear();
    findExtras(app);
}

void closeMenu(App& app);

void chooseFilesButton(App& app, int button) {
    if (button < 0 || button >= kFilesButtonCount) {
        return;
    }
    app.filesItem = button;
    if (button == 0) {
        pickFiles(app);
    } else if (inGame(app)) {
        app.filesOpen = false;
    } else if (canPlay(app)) {
        if (app.menuOpen) {
            closeMenu(app);
        }
        startGame(app);
    }
}

// The events on the game files screen; returns false for the ones it
// leaves alone, and sets `quit` when the player quits.
bool filesEvent(App& app, SDL_Event& event, bool& quit) {
    const auto back = [&] {
        if (inGame(app)) {
            app.filesOpen = false;
        } else {
            quit = true;
        }
    };
    switch (event.type) {
    case SDL_EVENT_KEY_DOWN:
        if (event.key.repeat && event.key.key != SDLK_LEFT && event.key.key != SDLK_RIGHT) {
            return true;
        }
        switch (event.key.key) {
        case SDLK_ESCAPE:
            back();
            break;
        case SDLK_LEFT:
        case SDLK_RIGHT:
        case SDLK_UP:
        case SDLK_DOWN:
        case SDLK_TAB:
        case SDLK_A:
        case SDLK_D:
            app.filesItem = 1 - app.filesItem;
            break;
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
        case SDLK_SPACE:
            chooseFilesButton(app, app.filesItem);
            break;
        default:
            break;
        }
        return true;
    case SDL_EVENT_MOUSE_MOTION: {
        SDL_ConvertEventToRenderCoordinates(app.renderer, &event);
        if (const int button = filesButtonAt(event.motion.x, event.motion.y); button >= 0) {
            app.filesItem = button;
        }
        return true;
    }
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        SDL_ConvertEventToRenderCoordinates(app.renderer, &event);
        if (event.button.button == SDL_BUTTON_LEFT) {
            chooseFilesButton(app, filesButtonAt(event.button.x, event.button.y));
        } else if (event.button.button == SDL_BUTTON_RIGHT && inGame(app)) {
            app.filesOpen = false;
        }
        return true;
    case SDL_EVENT_GAMEPAD_ADDED:
        SDL_OpenGamepad(event.gdevice.which);
        return true;
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        switch (event.gbutton.button) {
        case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
        case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
        case SDL_GAMEPAD_BUTTON_DPAD_UP:
        case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
            app.filesItem = 1 - app.filesItem;
            return true;
        default:
            break;
        }
        switch (app.buttons.action(event.gbutton.button)) {
        case mentat::PadAction::Quit:
            quit = true;
            break;
        case mentat::PadAction::Click:
            chooseFilesButton(app, app.filesItem);
            break;
        case mentat::PadAction::RightClick:
            if (inGame(app)) {
                app.filesOpen = false;
            }
            break;
        default:
            break;
        }
        return true;
    case SDL_EVENT_MOUSE_BUTTON_UP:
    case SDL_EVENT_KEY_UP:
    case SDL_EVENT_GAMEPAD_BUTTON_UP:
    case SDL_EVENT_MOUSE_WHEEL:
        return true;
    default:
        return false;
    }
}

void setColor(SDL_Renderer* r, Uint32 rgb) {
    SDL_SetRenderDrawColor(r, static_cast<Uint8>(rgb >> 16), static_cast<Uint8>(rgb >> 8), static_cast<Uint8>(rgb),
                           255);
}

constexpr Uint32 kGold = 0xE8C070;
constexpr Uint32 kText = 0xF0E6D2;
constexpr Uint32 kDim = 0xA09888;
constexpr Uint32 kGood = 0x90D070;
constexpr Uint32 kBad = 0xF07060;

// One file line: a mark, its name, how it stands and what it is.
float drawFileLine(SDL_Renderer* r, float y, const char* name, const std::string& state, Uint32 stateColor,
                   bool have, const std::string& about) {
    const SDL_FRect mark{kFilesLeft + 4, y, 7, 7};
    setColor(r, have ? kGood : kDim);
    if (have) {
        SDL_RenderFillRect(r, &mark);
    } else {
        SDL_RenderRect(r, &mark);
    }
    setColor(r, kText);
    SDL_RenderDebugText(r, kFilesLeft + 18, y, name);
    setColor(r, stateColor);
    const float stateX = kFilesRight - static_cast<float>(state.size()) * SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE;
    SDL_RenderDebugText(r, stateX, y, state.c_str());
    setColor(r, kDim);
    SDL_RenderDebugText(r, kFilesLeft + 18, y + 11, about.c_str());
    return y + 22;
}

void drawFiles(App& app) {
    SDL_Renderer* r = app.renderer;
    SDL_SetRenderDrawColor(r, 0x10, 0x0C, 0x08, 255);
    const SDL_FRect all{0, 0, kScreenW, kScreenH};
    SDL_RenderFillRect(r, &all);
    setColor(r, kGold);
    const SDL_FRect frame{4, 4, kScreenW - 8, kScreenH - 8};
    SDL_RenderRect(r, &frame);
    SDL_RenderDebugText(r, kFilesLeft, 13, (std::string(mentat::kProjectName) + " - game files").c_str());

    setColor(r, kGold);
    SDL_RenderDebugText(r, kFilesLeft, 31, "Needed to play");
    std::string gameState = app.gameFound || inGame(app) ? "Found" : "Missing";
    Uint32 gameColor = app.gameFound || inGame(app) ? kGood : kBad;
    if (app.copying) {
        const std::uint64_t total = app.toCopy;
        const std::uint64_t done = app.copied;
        gameState = "Copying " + std::to_string(total == 0 ? 0 : done * 100 / total) + "%";
        gameColor = kText;
    }
    float y = drawFileLine(r, 43, "Game CD", gameState, gameColor, app.gameFound || inGame(app),
                           "DUNE.DAT and DNCDPRG.EXE");
    if (app.copying && app.toCopy > 0) {
        const float width = kFilesRight - kFilesLeft - 18;
        SDL_SetRenderDrawColor(r, 0x50, 0x40, 0x30, 255);
        const SDL_FRect barBack{kFilesLeft + 18, y, width, 3};
        SDL_RenderFillRect(r, &barBack);
        setColor(r, kGold);
        const SDL_FRect bar{kFilesLeft + 18, y,
                            width * static_cast<float>(app.copied.load()) / static_cast<float>(app.toCopy.load()), 3};
        SDL_RenderFillRect(r, &bar);
    }

    setColor(r, kGold);
    SDL_RenderDebugText(r, kFilesLeft, 74, "Optional");
#ifdef MENTAT_WITH_MT32
    const std::string romState = app.romsFound ? "Found" : "Not added";
    const Uint32 romColor = app.romsFound ? kGood : kDim;
#else
    const std::string romState = "Not built in";
    const Uint32 romColor = kDim;
#endif
    drawFileLine(r, 86, "Roland MT-32", romState, romColor, app.romsFound, "Its two ROMs, for its music");

    for (int i = 0; i < kFilesButtonCount; ++i) {
        const SDL_FRect& b = kFilesButtons[i];
        const bool enabled = i == 0 ? !app.copying : canPlay(app);
        const char* label = i == 0 ? "Add files..." : (inGame(app) ? "Back" : "Play");
        if (i == app.filesItem && enabled) {
            SDL_SetRenderDrawColor(r, 0x50, 0x3C, 0x20, 255);
            SDL_RenderFillRect(r, &b);
        }
        setColor(r, enabled ? kGold : 0x504030);
        SDL_RenderRect(r, &b);
        setColor(r, enabled ? kText : 0x605848);
        const float width = static_cast<float>(std::strlen(label)) * SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE;
        SDL_RenderDebugText(r, b.x + (b.w - width) / 2, b.y + 4, label);
    }

    setColor(r, kDim);
    writeText(r, 117, "Drop your CD image (.iso, .bin), the CD or the ROMs on this window, or:",
                  kFilesLeft, static_cast<std::size_t>((kFilesRight - kFilesLeft) / SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE));
    if (!app.filesNote.empty()) {
        setColor(r, app.noteIsError ? kBad : kGood);
        writeText(r, 172, app.filesNote, kFilesLeft,
                  static_cast<std::size_t>((kFilesRight - kFilesLeft) / SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE));
    } else {
        setColor(r, kDim);
        SDL_RenderDebugText(r, kFilesLeft, 178, inGame(app) ? "Esc goes back." : "Esc quits.");
    }
}

// The largest whole scale at which the game fits in `usable`, leaving room
// for the desktop's panels and the window's title bar.
int scaleToFit(const SDL_Rect& usable) {
    if (usable.w <= 0 || usable.h <= 0) {
        return 3;
    }
    const int across = usable.w * 9 / 10 / kScreenW;
    const int down = usable.h * 9 / 10 / kScreenH;
    return std::max(1, std::min(across, down));
}

SDL_Rect usableBounds(SDL_DisplayID display) {
    SDL_Rect usable{};
    if (display == 0 || !SDL_GetDisplayUsableBounds(display, &usable)) {
        usable = SDL_Rect{0, 0, 0, 0};
    }
    return usable;
}

// Makes the window as large as fits on the screen it is on, and puts it in
// the middle: on start, and when it moves to another screen or the screen
// changes. Full screen is left alone.
void fitWindow(SDL_Window* window) {
    if ((SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0) {
        return;
    }
    const SDL_Rect usable = usableBounds(SDL_GetDisplayForWindow(window));
    if (usable.w <= 0) {
        return;
    }
    const int scale = scaleToFit(usable);
    const int w = kScreenW * scale;
    const int h = kScreenH * scale;
    SDL_SetWindowSize(window, w, h);
    SDL_SetWindowPosition(window, usable.x + (usable.w - w) / 2, usable.y + (usable.h - h) / 2);
}

// A window that no longer fits on its screen is made to fit again.
void keepOnScreen(SDL_Window* window) {
    if ((SDL_GetWindowFlags(window) & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_MAXIMIZED)) != 0) {
        return;
    }
    const SDL_Rect usable = usableBounds(SDL_GetDisplayForWindow(window));
    int w = 0;
    int h = 0;
    SDL_GetWindowSize(window, &w, &h);
    if (usable.w > 0 && (w > usable.w || h > usable.h)) {
        fitWindow(window);
    }
}

bool isModifier(SDL_Keycode key) {
    switch (key) {
    case SDLK_LSHIFT:
    case SDLK_RSHIFT:
    case SDLK_LCTRL:
    case SDLK_RCTRL:
    case SDLK_LALT:
    case SDLK_RALT:
    case SDLK_LGUI:
    case SDLK_RGUI:
    case SDLK_MODE:
    case SDLK_CAPSLOCK:
    case SDLK_NUMLOCKCLEAR:
    case SDLK_SCROLLLOCK:
        return true;
    default:
        return false;
    }
}

void toggleFullscreen(SDL_Window* window) {
    const bool full = (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0;
    SDL_SetWindowFullscreen(window, !full);
}

// Window keys that work everywhere: F11 or Alt+Enter for full screen.
bool windowKey(App& app, const SDL_KeyboardEvent& key) {
    const bool enter = key.key == SDLK_RETURN || key.key == SDLK_KP_ENTER;
    if (key.key == SDLK_F11 || (enter && (key.mod & SDL_KMOD_ALT) != 0)) {
        if (!key.repeat) {
            toggleFullscreen(app.window);
        }
        return true;
    }
    return false;
}

// --- Mentat's options menu (F2, or Start on a gamepad). The game waits
// while it is open.

enum class MenuKind { Music, Files, Back };

struct MenuItem {
    const char* label;
    MenuKind kind;
    mentat::MusicChoice music; // for the music lines
};

constexpr MenuItem kMenuItems[] = {
    {"AdLib", MenuKind::Music, mentat::MusicChoice::AdLib},
    {"AdLib Gold", MenuKind::Music, mentat::MusicChoice::AdLibGold},
    {"Roland MT-32", MenuKind::Music, mentat::MusicChoice::Mt32},
    {"Game files...", MenuKind::Files, mentat::MusicChoice::AdLib},
    {"Back to the game", MenuKind::Back, mentat::MusicChoice::AdLib},
};
constexpr int kMenuCount = static_cast<int>(std::size(kMenuItems));
constexpr float kMenuLeft = 40;
constexpr float kMenuTop = 36;
constexpr float kMenuWidth = kScreenW - 2 * kMenuLeft;
constexpr float kMenuHeight = 150;
constexpr float kItemTop = kMenuTop + 44;
constexpr float kItemHeight = 13;

float menuItemY(int item) {
    // A gap after the music.
    return kItemTop + static_cast<float>(item) * kItemHeight +
           (kMenuItems[item].kind == MenuKind::Music ? 0.0f : 8.0f);
}

int menuItemAt(float x, float y) {
    for (int i = 0; i < kMenuCount; ++i) {
        const float top = menuItemY(i) - 3;
        if (x >= kMenuLeft + 4 && x < kMenuLeft + kMenuWidth - 4 && y >= top && y < top + kItemHeight) {
            return i;
        }
    }
    return -1;
}

void openMenu(App& app) {
    app.menuOpen = true;
    app.menuOpenedNs = SDL_GetTicksNS();
    app.menuNote.clear();
    app.menuItem = 0;
    for (int i = 0; i < kMenuCount; ++i) {
        if (kMenuItems[i].kind == MenuKind::Music && kMenuItems[i].music == app.options.music) {
            app.menuItem = i;
        }
    }
    SDL_ShowCursor();
}

void closeMenu(App& app) {
    app.menuOpen = false;
    app.filesOpen = false;
    // The game's clock goes on from where it stopped.
    app.startNs += SDL_GetTicksNS() - app.menuOpenedNs;
    app.lastFrameNs = 0;
    app.systemCursor = true;
}

void chooseMusic(App& app, mentat::MusicChoice choice) {
    if (choice == app.options.music) {
        return;
    }
    if (app.archive) {
        app.menuNote.clear();
        auto next = makeMusic(app, choice);
        if (next == nullptr) {
            if (app.menuNote.empty()) {
                app.menuNote = "That music cannot play here.";
            }
            return;
        }
        if (app.liveGame != nullptr) {
            // The game thread waits while the menu is open, so it can be
            // handed the new player here.
            app.liveGame->switchMusic(next.get());
        }
        app.music = std::move(next);
    }
    app.options.music = choice;
    writeOptions(app);
}

void chooseMenuItem(App& app, int item) {
    if (item < 0 || item >= kMenuCount) {
        return;
    }
    app.menuItem = item;
    switch (kMenuItems[item].kind) {
    case MenuKind::Music:
        chooseMusic(app, kMenuItems[item].music);
        break;
    case MenuKind::Files:
        if (inGame(app)) {
            openFiles(app);
        } else {
            closeMenu(app); // the game files screen is behind the menu
        }
        break;
    case MenuKind::Back:
        closeMenu(app);
        break;
    }
}

void moveMenu(App& app, int by) {
    app.menuItem = (app.menuItem + by + kMenuCount) % kMenuCount;
}

// The events while the menu is open; returns false for the ones it leaves
// alone (quitting, windows).
bool menuEvent(App& app, SDL_Event& event) {
    switch (event.type) {
    case SDL_EVENT_KEY_DOWN:
        switch (event.key.key) {
        case SDLK_F2:
        case SDLK_ESCAPE:
            if (!event.key.repeat) {
                closeMenu(app);
            }
            break;
        case SDLK_UP:
        case SDLK_W:
            moveMenu(app, -1);
            break;
        case SDLK_DOWN:
        case SDLK_S:
            moveMenu(app, 1);
            break;
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
        case SDLK_SPACE:
            if (!event.key.repeat) {
                chooseMenuItem(app, app.menuItem);
            }
            break;
        default:
            break;
        }
        return true;
    case SDL_EVENT_MOUSE_MOTION: {
        SDL_ConvertEventToRenderCoordinates(app.renderer, &event);
        if (const int item = menuItemAt(event.motion.x, event.motion.y); item >= 0) {
            app.menuItem = item;
        }
        return true;
    }
    case SDL_EVENT_MOUSE_BUTTON_DOWN: {
        SDL_ConvertEventToRenderCoordinates(app.renderer, &event);
        const float x = event.button.x;
        const float y = event.button.y;
        if (event.button.button != SDL_BUTTON_LEFT || x < kMenuLeft || x >= kMenuLeft + kMenuWidth ||
            y < kMenuTop || y >= kMenuTop + kMenuHeight) {
            closeMenu(app);
        } else {
            chooseMenuItem(app, menuItemAt(x, y));
        }
        return true;
    }
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        switch (event.gbutton.button) {
        case SDL_GAMEPAD_BUTTON_DPAD_UP:
            moveMenu(app, -1);
            return true;
        case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
            moveMenu(app, 1);
            return true;
        default:
            break;
        }
        switch (app.buttons.action(event.gbutton.button)) {
        case mentat::PadAction::Quit:
            return false;
        case mentat::PadAction::Click:
            chooseMenuItem(app, app.menuItem);
            break;
        case mentat::PadAction::RightClick:
        case mentat::PadAction::Menu:
            closeMenu(app);
            break;
        default:
            break;
        }
        return true;
    case SDL_EVENT_MOUSE_BUTTON_UP:
    case SDL_EVENT_KEY_UP:
    case SDL_EVENT_GAMEPAD_BUTTON_UP:
    case SDL_EVENT_MOUSE_WHEEL:
        return true;
    default:
        return false;
    }
}

void drawMenu(App& app) {
    SDL_Renderer* r = app.renderer;
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 0, 0, 0, 0x90);
    const SDL_FRect all{0, 0, kScreenW, kScreenH};
    SDL_RenderFillRect(r, &all);
    SDL_SetRenderDrawColor(r, 0x18, 0x12, 0x0C, 0xF0);
    const SDL_FRect box{kMenuLeft, kMenuTop, kMenuWidth, kMenuHeight};
    SDL_RenderFillRect(r, &box);
    SDL_SetRenderDrawColor(r, 0xE8, 0xC0, 0x70, 255);
    SDL_RenderRect(r, &box);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);

    const float left = kMenuLeft + 10;
    SDL_RenderDebugText(r, left, kMenuTop + 10, "Mentat options");
    SDL_SetRenderDrawColor(r, 0xA0, 0x98, 0x88, 255);
    SDL_RenderDebugText(r, left, kMenuTop + 28, "Music");
    for (int i = 0; i < kMenuCount; ++i) {
        const MenuItem& item = kMenuItems[i];
        const float y = menuItemY(i);
        if (i == app.menuItem) {
            SDL_SetRenderDrawColor(r, 0x50, 0x3C, 0x20, 255);
            const SDL_FRect bar{kMenuLeft + 4, y - 3, kMenuWidth - 8, kItemHeight};
            SDL_RenderFillRect(r, &bar);
        }
        std::string text;
        if (item.kind == MenuKind::Music) {
            text = std::string(item.music == app.options.music ? "(*) " : "( ) ") + item.label;
        } else {
            text = item.label;
        }
        SDL_SetRenderDrawColor(r, 0xF0, 0xE6, 0xD2, 255);
        SDL_RenderDebugText(r, left, y, text.c_str());
    }
    if (!app.menuNote.empty()) {
        SDL_SetRenderDrawColor(r, 0xF0, 0x90, 0x70, 255);
        writeText(r, menuItemY(kMenuCount - 1) + 14, app.menuNote, left,
                  static_cast<std::size_t>((kMenuWidth - 20) / SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE));
    }
}

SDL_AppResult SDL_AppInit(void** state, int argc, char** argv) {
    auto app = std::make_unique<App>();
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    const std::string title(mentat::kProjectName);
    const int scale = scaleToFit(usableBounds(SDL_GetPrimaryDisplay()));
    if (!SDL_CreateWindowAndRenderer(title.c_str(), kScreenW * scale, kScreenH * scale, SDL_WINDOW_RESIZABLE,
                                     &app->window, &app->renderer)) {
        SDL_Log("cannot open a window: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    fitWindow(app->window);
    SDL_SetRenderLogicalPresentation(app->renderer, kScreenW, kScreenH, SDL_LOGICAL_PRESENTATION_LETTERBOX);
    SDL_SetRenderVSync(app->renderer, 1);
    app->texture =
        SDL_CreateTexture(app->renderer, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STREAMING, kScreenW, kScreenH);
    SDL_SetTextureScaleMode(app->texture, SDL_SCALEMODE_NEAREST);
    openAudio(*app);
    readPadButtons(*app);
    readOptions(*app);

    app->argc = argc;
    app->argv = argv;
    openGameFiles(*app, argc, argv);
    app->gameFound = app->problem.empty();
    findExtras(*app);
    if (app->gameFound) {
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
    if (event->type == SDL_EVENT_KEY_DOWN && windowKey(*app, event->key)) {
        return SDL_APP_CONTINUE;
    }
    if (event->type == SDL_EVENT_DROP_FILE) {
        if (event->drop.data != nullptr) {
            const std::lock_guard lock(app->pickedLock);
            app->picked.emplace_back(event->drop.data);
        }
        return SDL_APP_CONTINUE;
    }
    if (app->filesOpen && app->menuOpen) {
        bool quit = false;
        if (filesEvent(*app, *event, quit)) {
            return quit ? SDL_APP_SUCCESS : SDL_APP_CONTINUE;
        }
    } else if (app->menuOpen) {
        if (menuEvent(*app, *event)) {
            return SDL_APP_CONTINUE;
        }
    } else if ((event->type == SDL_EVENT_KEY_DOWN && event->key.key == SDLK_F2 && !event->key.repeat) ||
               (event->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN &&
                app->buttons.action(event->gbutton.button) == mentat::PadAction::Menu)) {
        if (!app->copying) {
            openMenu(*app);
        }
        return SDL_APP_CONTINUE;
    }
    if (event->type == SDL_EVENT_WINDOW_DISPLAY_CHANGED || event->type == SDL_EVENT_DISPLAY_CURRENT_MODE_CHANGED ||
        event->type == SDL_EVENT_DISPLAY_USABLE_BOUNDS_CHANGED) {
        keepOnScreen(app->window);
        return SDL_APP_CONTINUE;
    }
    if (app->game == nullptr) {
        bool quit = event->type == SDL_EVENT_QUIT;
        filesEvent(*app, *event, quit);
        return quit ? SDL_APP_SUCCESS : SDL_APP_CONTINUE;
    }
    switch (event->type) {
    case SDL_EVENT_QUIT:
        return SDL_APP_SUCCESS;
    case SDL_EVENT_KEY_DOWN:
        if (event->key.key == SDLK_ESCAPE && app->game == nullptr) {
            return SDL_APP_SUCCESS;
        }
        // Shift, Ctrl, Alt and the system key alone are not presses for the
        // game: switching windows with Alt+Tab must not skip the opening.
        if (isModifier(event->key.key)) {
            return SDL_APP_CONTINUE;
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
        case mentat::PadAction::Menu:
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
    checkCopy(*app);
    if (app->game != nullptr && !app->menuOpen) {
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
        drawFiles(*app);
    }
    if (app->menuOpen) {
        if (app->filesOpen) {
            drawFiles(*app);
        } else {
            drawMenu(*app);
        }
    }
    SDL_RenderPresent(app->renderer);
    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* state, SDL_AppResult) {
    delete static_cast<App*>(state);
}
