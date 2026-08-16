#pragma once
#include <string>
#include <vector>
#include <map>
//returns the list of names (not paths) in the watched folder
std::vector<std::string> list_files(const std::string & folder_path);
std::vector<char> read_file_bytes(const std::string& file_path);

enum class ChangeType { New, Modified, Unchanged };

struct FileChange {
    std::string filename;
    ChangeType type;
};

// Compares current folder contents against known_hashes, updates known_hashes,
// and returns the list of files that are new or modified (skips unchanged).
std::vector<FileChange> detect_changes(const std::string& folder_path,
         std::map<std::string, std::string>& known_hashes);