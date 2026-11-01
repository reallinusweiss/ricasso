#include "platform/io_thread.h"
#include <chrono>
#include <filesystem>
#include <iostream>
#include <thread>

int main() {
    namespace fs = std::filesystem;
    const auto path = fs::temp_directory_path() /
        ("swordigo-async-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".bin");
    const std::vector<uint8_t> expected{0, 10, 13, 255, 42};
    bool done = false, success = false;
    const auto caller = std::this_thread::get_id();
    io_thread_start();
    // Let the worker enter its empty-queue condition-variable wait first.
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    io_thread_post_save(path.u8string(), expected);
    io_thread_post_load(path.u8string(), [&](bool ok, std::vector<uint8_t> data) {
        success = ok && data == expected && std::this_thread::get_id() == caller;
        done = true;
    });
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (!done && std::chrono::steady_clock::now() < deadline) {
        io_thread_poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    io_thread_stop();
    fs::remove(path);
    if (!done || !success) return 1;
    std::cout << "Async queue wake, binary save/load and main-thread callback passed\n";
    return 0;
}
