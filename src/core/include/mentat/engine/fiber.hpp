// SPDX-License-Identifier: GPL-3.0-or-later
//
// Runs the game on its own thread, in lock step with the app.
//
// The original is one long program that waits for time to pass in many
// places (in the middle of a video, a fade or a conversation). Mentat keeps
// that shape: the game code runs on a thread of its own and calls yield()
// when it waits. The app calls resume() once per frame. Only one of the two
// threads ever runs at a time, so nothing they share needs a lock, and a run
// with the same input always does the same thing.
#pragma once

#include <condition_variable>
#include <exception>
#include <functional>
#include <mutex>
#include <thread>

namespace mentat {

// Thrown inside the game thread by yield() when the app is closing, so the
// game code unwinds cleanly.
struct StopRequested {};

class Fiber {
public:
    explicit Fiber(std::function<void()> body);
    ~Fiber(); // stops the game thread if it still runs

    Fiber(const Fiber&) = delete;
    Fiber& operator=(const Fiber&) = delete;

    // App side: lets the game run until it yields or ends. Rethrows an
    // exception the game ended with.
    void resume();
    bool finished() const { return finished_; }

    // Game side: hands control back to the app until the next resume().
    void yield();

private:
    void run();

    std::function<void()> body_;
    std::mutex mutex_;
    std::condition_variable wake_;
    bool gameTurn_ = false;
    bool stop_ = false;
    bool started_ = false;
    bool finished_ = false;
    std::exception_ptr error_;
    std::thread thread_;
};

} // namespace mentat
