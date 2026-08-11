#include "watcher.hpp"
#include <iostream>
#include <thread>
#include <chrono>

int main() {
    std::string folder = "../sync_folder";

    while (true) {
        std::cout << "--- Scanning " << folder << " ---\n";
        auto files = list_files(folder);

        for (const auto& f : files) {
            std::cout << "Found: " << f << "\n";
        }

        std::this_thread::sleep_for(std::chrono::seconds(5));
    }

    return 0;
}