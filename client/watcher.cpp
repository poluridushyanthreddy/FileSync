#include "watcher.hpp"
#include "hashing.hpp"
#include <filesystem>
#include <fstream>
#include <set>
#include <map>

namespace fs = std::filesystem;

std::vector<std::string> list_files(const std::string& folder_path) {
    std::vector<std::string> files;

    if (!fs::exists(folder_path)) {
        return files;
    }
    
    for (const auto& entry : fs::directory_iterator(folder_path)) {
        if (entry.is_regular_file()) {
            std::string name = entry.path().filename().string();
            if (!name.empty() && name[0] == '.') {
                continue; // skip dotfiles like .gitkeep
            }
            files.push_back(name);
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
                                         std::map<std::string, FileState>& known_state) {
    std::vector<FileChange> changes;
    auto files = list_files(folder_path);

    // Build a quick lookup of what's currently on disk
    std::set<std::string> current_files(files.begin(), files.end());

    for (const auto& filename : files) {
        auto bytes = read_file_bytes(folder_path + "/" + filename);
        std::string hash = compute_sha256(bytes);

        auto it = known_state.find(filename);

        if (it == known_state.end()) {
            changes.push_back({filename, ChangeType::New});
            known_state[filename] = FileState{hash,0};// version unknown until server confirms
        } else if (it->second.hash != hash) {
            changes.push_back({filename, ChangeType::Modified});
            it->second.hash=hash;// keep existing version — it's the *base* version for this upload
        }
        // else unchanged — skip, no update needed
    }

    // Pass 2: detect deletions — known files no longer present on disk
    for(auto it=known_state.begin();it!=known_state.end();){
        if(current_files.find(it->first)==current_files.end()){
            changes.push_back({it->first,ChangeType::Deleted});
            it=known_state.erase(it);
        }
        else ++it;
    }

    return changes;
}