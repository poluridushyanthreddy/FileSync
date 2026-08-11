#pragma once
#include <string>
#include <vector>

//returns the list of names (not paths) in the watched folder
std::vector<std::string> list_files(const std::string & folder_path);
