// SPDX-License-Identifier: GPL-3.0-or-later
#include "mentat/engine/fiber.hpp"

#include <utility>

namespace mentat {

Fiber::Fiber(std::function<void()> body) : body_(std::move(body)) {}

Fiber::~Fiber() {
    if (!started_) {
        return;
    }
    {
        std::unique_lock lock(mutex_);
        stop_ = true;
        gameTurn_ = true;
    }
    wake_.notify_all();
    thread_.join();
}

void Fiber::run() {
    {
        std::unique_lock lock(mutex_);
        wake_.wait(lock, [this] { return gameTurn_; });
    }
    try {
        if (!stop_) {
            body_();
        }
    } catch (const StopRequested&) {
        // The app is closing.
    } catch (...) {
        error_ = std::current_exception();
    }
    {
        std::unique_lock lock(mutex_);
        finished_ = true;
        gameTurn_ = false;
    }
    wake_.notify_all();
}

void Fiber::resume() {
    if (finished_) {
        return;
    }
    if (!started_) {
        started_ = true;
        thread_ = std::thread([this] { run(); });
    }
    std::unique_lock lock(mutex_);
    gameTurn_ = true;
    wake_.notify_all();
    wake_.wait(lock, [this] { return !gameTurn_; });
    if (error_) {
        auto error = std::exchange(error_, nullptr);
        std::rethrow_exception(error);
    }
}

void Fiber::yield() {
    std::unique_lock lock(mutex_);
    gameTurn_ = false;
    wake_.notify_all();
    wake_.wait(lock, [this] { return gameTurn_; });
    if (stop_) {
        throw StopRequested{};
    }
}

} // namespace mentat
