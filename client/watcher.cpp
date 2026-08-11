#include "watcher.hpp"
#include <filesystem>

namespace fs=std::filesystem;
std::vector<std::string> list_files(const std::string& folder_path){
    std::vector<std::string> files;

    for(const auto& entry:fs::directory_iterator(folder_path)){
        if(entry.is_regular_file()){
            files.push_back(entry.path().filename().string());
        }
    }
    return files;
}