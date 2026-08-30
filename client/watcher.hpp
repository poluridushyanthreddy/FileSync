#pragma once
#include <string>
#include <vector>
#include <map>
//returns the list of names (not paths) in the watched folder
std::vector<std::string> list_files(const std::string & folder_path);
std::vector<char> read_file_bytes(const std::string& file_path);

enum class ChangeType { New, Modified, Deleted };

struct FileChange {
    std::string filename;
    ChangeType type;
};

struct FileState{
    std::string hash;
    int version=0;
};

// Compares current folder contents against known_state, updates known_state,
// and returns the list of files that are new or modified (skips unchanged).
std::vector<FileChange> detect_changes(const std::string& folder_path,
         std::map<std::string, FileState>& known_state);