#include "watcher.hpp"
#include "hashing.hpp"
#include <filesystem>
#include <fstream>
#include <map>

namespace fs = std::filesystem;

std::vector<std::string> list_files(const std::string& folder_path) {
    std::vector<std::string> files;
    for (const auto& entry : fs::directory_iterator(folder_path)) {
        if (entry.is_regular_file()) {
            files.push_back(entry.path().filename().string());
        }
    }
    return files;
}

std::vector<char> read_file_bytes(const std::string& file_path) {
    std::ifstream infile(file_path, std::ios::binary);
    return std::vector<char>((std::istreambuf_iterator<char>(infile)),
                               std::istreambuf_iterator<char>());
}

std::vector<FileChange> detect_changes(const std::string& folder_path,
                                         std::map<std::string, std::string>& known_hashes) {
    std::vector<FileChange> changes;
    auto files = list_files(folder_path);

    for (const auto& filename : files) {
        auto bytes = read_file_bytes(folder_path + "/" + filename);
        std::string hash = compute_sha256(bytes);

        auto it = known_hashes.find(filename);

        if (it == known_hashes.end()) {
            changes.push_back({filename, ChangeType::New});
            known_hashes[filename] = hash;
        } else if (it->second != hash) {
            changes.push_back({filename, ChangeType::Modified});
            known_hashes[filename] = hash;
        }
        // else unchanged — skip, no update needed
    }

    return changes;
}