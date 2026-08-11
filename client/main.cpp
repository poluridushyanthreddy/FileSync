#include <iostream>
#include <fstream>
#include <vector>
#include <boost/asio.hpp>
#include <nlohmann/json.hpp>
#include <openssl/sha.h>
#include <sstream>
#include <iomanip>

using boost::asio::ip::tcp;
using json=nlohmann::json;

std::string compute_sha(const::std::vector<char>& data){
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(data.data()),data.size(),hash);

    std::ostringstream oss;
    for(int i=0;i<SHA256_DIGEST_LENGTH;++i){
        oss<<std::hex<<std::setw(2)<<std::setfill('0')<<static_cast<int>(hash[i]);
    }
    return oss.str();
}

//argc is is the count of no of arguments and argv[] is an array of cstrings
int main(int argc,char* argv[])
{
    try{
        if(argc<2){
            std::cerr<<"Usage: "<<argv[0]<<" <filename>\n";
            return 1;
        }
        std::string filename=argv[1];

        //read file into memory
        std::ifstream infile(filename,std::ios::binary);
        if(!infile)
        {
            std::cerr<<"Could not open: "<<filename<<"\n";
            return 1;
        }
        std::vector<char> file_data((std::istreambuf_iterator<char>(infile)),
                    std::istreambuf_iterator<char>());
        infile.close();

        std::cout<<"Read "<<file_data.size()<<" bytes from "<<filename<<"\n";

        std::string file_hash=compute_sha(file_data);
        std::cout<<"SHA-256: "<<file_hash<<"\n";

        //connect to server
        boost::asio::io_context io_context;
        tcp::resolver resolver(io_context);
        auto endpoints=resolver.resolve("127.0.0.1","9000");

        tcp::socket socket(io_context);
        boost::asio::connect(socket,endpoints);

        std::cout<< "Connected to server. \n";

        //Build header
        json header;
        header["type"]="FILE_UPLOAD";
        header["filename"]=filename;
        header["size"]=file_data.size();
        header["hash"]=file_hash;

        std::string header_str=header.dump()+"\n";

        // Send header, then file bytes
        boost::asio::write(socket, boost::asio::buffer(header_str));
        boost::asio::write(socket, boost::asio::buffer(file_data));

        // Read the ACK
        boost::asio::streambuf response_buf;
        boost::asio::read_until(socket, response_buf,"\n");

        std::istream is(&response_buf);
        std::string ack_line;
        std::getline(is,ack_line);

        std::cout<<"Server ACK: "<<ack_line<<"\n";
    }
    catch(std::exception& e)
    {
        std::cerr<<"Exception: "<<e.what()<<"\n";
    }
    return 0;
}