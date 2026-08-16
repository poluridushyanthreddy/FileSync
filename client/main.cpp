#include <iostream>
#include <fstream>
#include <vector>
#include <boost/asio.hpp>
#include <nlohmann/json.hpp>
#include "hashing.hpp"
#include "watcher.hpp"
#include <thread>
#include <chrono>
#include <map>

using boost::asio::ip::tcp;
using json=nlohmann::json;

void upload_file(const std::string& folder_path, const std::string& filename) {
    try {
        std::string full_path = folder_path + "/" + filename;

        std::ifstream infile(full_path, std::ios::binary);
        if (!infile) {
            std::cerr << "Could not open file: " << full_path << "\n";
            return;
        }
        std::vector<char> file_data((std::istreambuf_iterator<char>(infile)),
                                      std::istreambuf_iterator<char>());
        infile.close();

        std::string file_hash = compute_sha256(file_data);

        boost::asio::io_context io_context;
        tcp::resolver resolver(io_context);
        auto endpoints = resolver.resolve("127.0.0.1", "9000");

        tcp::socket socket(io_context);
        boost::asio::connect(socket, endpoints);

        json header;
        header["type"] = "FILE_UPLOAD";
        header["filename"] = filename;
        header["size"] = file_data.size();
        header["hash"] = file_hash;

        std::string header_str = header.dump() + "\n";

        boost::asio::write(socket, boost::asio::buffer(header_str));
        boost::asio::write(socket, boost::asio::buffer(file_data));

        boost::asio::streambuf response_buf;
        boost::asio::read_until(socket, response_buf, "\n");

        std::istream is(&response_buf);
        std::string ack_line;
        std::getline(is, ack_line);

        std::cout << "Uploaded " << filename << " -> " << ack_line << "\n";

    } catch (std::exception& e) {
        std::cerr << "Exception uploading " << filename << ": " << e.what() << "\n";
    }
}

//argc is is the count of no of arguments and argv[] is an array of cstrings
int main()
{
    std::string folder = "../sync_folder";
    std::map<std::string, std::string> known_hashes;

    std::cout << "Watching " << folder << " for changes...\n";

    while (true) {
        auto changes = detect_changes(folder, known_hashes);

        for (const auto& change : changes) {
            std::cout << (change.type == ChangeType::New ? "NEW" : "MODIFIED")
                      << ": " << change.filename << " -> uploading...\n";
            upload_file(folder, change.filename);
        }

        std::this_thread::sleep_for(std::chrono::seconds(5));
    }

    return 0;
}