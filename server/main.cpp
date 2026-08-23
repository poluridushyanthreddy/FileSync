#include <iostream>
#include <boost/asio.hpp>
#include <fstream>
#include <nlohmann/json.hpp>
#include <ctime>
#include <thread>
#include "database.hpp"

using boost::asio::ip::tcp;
using json=nlohmann::json;

void handle_client(tcp::socket socket,Database& db) {
    try {
        boost::asio::streambuf buf;
        boost::system::error_code error;

        boost::asio::read_until(socket, buf, "\n", error);
        if (error) {
            std::cerr << "Read error: " << error.message() << "\n";
            return;
        }

        std::istream is(&buf);
        std::string header_line;
        std::getline(is, header_line);

        if (header_line.empty()) {
            std::cerr << "Received empty header line — client likely disconnected early.\n";
            return;
        }

        std::cout << "Raw header line: " << header_line << "\n";

        json header;
        try {
            header = json::parse(header_line);
        } catch (json::parse_error& e) {
            std::cerr << "Malformed JSON header: " << e.what() << "\n";
            return;
        }

        if (!header.contains("type") || !header["type"].is_string()) {
            std::cerr << "Header missing valid 'type' field, dropping connection.\n";
            return;
        }

        std::string type=header["type"];

        if (type == "FILE_DELETE") {
            std::string filename = header["filename"];
            std::cout << "Delete requested for: " << filename << "\n";

            db.delete_file(filename);

            std::string filepath = "../storage/" + filename;
            if (std::filesystem::exists(filepath)) {
                std::filesystem::remove(filepath);
                std::cout << "Removed file: " << filepath << "\n";
            } else {
                std::cout << "File already absent: " << filepath << "\n";
            }

            json ack;
            ack["type"] = "ACK";
            ack["status"] = "ok";
            std::string ack_str = ack.dump() + "\n";
            boost::asio::write(socket, boost::asio::buffer(ack_str));
            return;
        }

        std::string filename = header["filename"];
        size_t file_size = header["size"];
        std::string file_hash=header["hash"];

        std::cout << "Parsed -> filename: " << filename
                  << ", size: " << file_size << "\n";

        std::string file_data;
        file_data.reserve(file_size);

        size_t already_have = buf.size();
        if (already_have > 0) {
            std::vector<char> temp(already_have);
            is.read(temp.data(), already_have);
            file_data.append(temp.data(), already_have);
        }

        if (file_data.size() < file_size) {
            size_t remaining = file_size - file_data.size();
            std::vector<char> temp(remaining);
            boost::asio::read(socket, boost::asio::buffer(temp), error);
            if (error) {
                std::cerr << "Read error: " << error.message() << "\n";
                return;
            }
            file_data.append(temp.data(), remaining);
        }

        std::ofstream outfile("../storage/" + filename, std::ios::binary);
        outfile.write(file_data.data(), file_data.size());
        outfile.close();

        long modified_at = static_cast<long>(std::time(nullptr));
        int version = db.upsert_file(filename, file_hash, file_data.size(), modified_at);

        if (version == -1) {
            std::cout << "Hash unchanged — no DB update needed.\n";
        } else {
            std::cout << "DB updated. New version: " << version << "\n";
        }

        std::cout << "Saved " << file_data.size() << " bytes to storage/"
                  << filename << "\n";

        json ack;
        ack["type"] = "ACK";
        ack["status"] = "ok";
        if(version!=-1) ack["version"]=version;
        std::string ack_str = ack.dump() + "\n";
        boost::asio::write(socket, boost::asio::buffer(ack_str));

    } catch (std::exception& e) {
        std::cerr << "Exception in handle_client: " << e.what() << "\n";
    }
}

int main()
{
    try
    {
        //The Engine that handles all I/O
        boost::asio::io_context io_context;
        Database db("../database/sync.db");

        //The host who listens on all network interfaces, port=9000
        tcp::acceptor acceptor(io_context,tcp::endpoint(tcp::v4(),9000));

        while(true){
        //Acceptor blocks until a client connects and then assigns a socket to the client
        tcp::socket socket(io_context);
        acceptor.accept(socket);

        std::cout<<"Client Connected\n";

        std::thread client_thread(handle_client, std::move(socket), std::ref(db));
        client_thread.detach();
        }
        }
    catch(std::exception& e)
    {
        std::cerr<<"Exception: "<<e.what()<<"\n";
    }
    return 0;
}