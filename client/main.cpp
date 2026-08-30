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

int upload_file(const std::string& folder_path, const std::string& filename,int base_version) {
    try {
        std::string full_path = folder_path + "/" + filename;

        std::ifstream infile(full_path, std::ios::binary);
        if (!infile) {
            std::cerr << "Could not open file: " << full_path << "\n";
            return -1;
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
        header["base_version"]=base_version;

        std::string header_str = header.dump() + "\n";

        boost::asio::write(socket, boost::asio::buffer(header_str));
        boost::asio::write(socket, boost::asio::buffer(file_data));

        boost::asio::streambuf response_buf;
        boost::asio::read_until(socket, response_buf, "\n");

        std::istream is(&response_buf);
        std::string ack_line;
        std::getline(is, ack_line);

        std::cout << "Uploaded " << filename << " -> " << ack_line << "\n";

        json ack = json::parse(ack_line);
        std::string status = ack.value("status", "");

        if (status == "conflict") {
            std::cerr << "CONFLICT on " << filename << " — server is at version "
                       << ack.value("current_version", -1) << "\n";
            return -2; // sentinel: conflict
        }

        return ack.value("version", base_version); // if unchanged (no "version" key), keep old version


    } catch (std::exception& e) {
        std::cerr << "Exception uploading " << filename << ": " << e.what() << "\n";
        return -1;
    }
}

void send_deleted(const std::string& filename){
    try {
        boost::asio::io_context io_context;
        tcp::resolver resolver(io_context);
        auto endpoints = resolver.resolve("127.0.0.1", "9000");

        tcp::socket socket(io_context);
        boost::asio::connect(socket, endpoints);

        json header;
        header["type"] = "FILE_DELETE";
        header["filename"] = filename;

        std::string header_str = header.dump() + "\n";
        boost::asio::write(socket, boost::asio::buffer(header_str));

        boost::asio::streambuf response_buf;
        boost::asio::read_until(socket, response_buf, "\n");

        std::istream is(&response_buf);
        std::string ack_line;
        std::getline(is, ack_line);

        std::cout << "Deleted " << filename << " -> " << ack_line << "\n";

    } catch (std::exception& e) {
        std::cerr << "Exception deleting " << filename << ": " << e.what() << "\n";
    }
}

int main(int argc, char* argv[])
{
    std::string folder = (argc>1)?argv[1]:"../sync_folder";
    std::map<std::string, FileState> known_state;

    std::cout << "Watching " << folder << " for changes...\n";

    while (true) {
        auto changes = detect_changes(folder, known_state);

        for (const auto& change : changes) {
            if (change.type == ChangeType::Deleted) {
                std::cout << "DELETED: " << change.filename << " -> notifying server...\n";
                send_deleted(change.filename);
            } else {
                std::string type_str = (change.type == ChangeType::New) ? "NEW" : "MODIFIED";
                std::cout << type_str << ": " << change.filename << " -> uploading...\n";

                int base_version = known_state[change.filename].version;
                int result = upload_file(folder, change.filename, base_version);

                if (result >= 0) {
                    known_state[change.filename].version = result;
                }
            }
        }

        std::this_thread::sleep_for(std::chrono::seconds(5));
    }

    return 0;
}