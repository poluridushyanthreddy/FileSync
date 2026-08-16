#include "watcher.hpp"
#include <iostream>
#include <thread>
#include <chrono>

int main() {
    std::string folder = "../sync_folder";
    std::map<std::string, std::string> known_hashes;

    while (true) {
        std::cout << "--- Scanning " << folder << " ---\n";
        auto changes = detect_changes(folder,known_hashes);

        for (const auto& change : changes) {
            std::string type_str = (change.type == ChangeType::New) ? "NEW" : "MODIFIED";
            std::cout << type_str << ": " << change.filename << "\n";
        }

        std::this_thread::sleep_for(std::chrono::seconds(5));
    }

    return 0;
}