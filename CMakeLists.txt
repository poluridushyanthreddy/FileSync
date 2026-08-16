cmake_minimum_required(VERSION 3.16)
project(FileSync)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(SQLite3 REQUIRED)

add_executable(server server/main.cpp server/database.cpp)
target_link_libraries(server PRIVATE Boost::boost OpenSSL::Crypto nlohmann_json::nlohmann_json SQLite::SQLite3)

add_executable(client client/main.cpp client/hashing.cpp client/watcher.cpp)
target_link_libraries(client PRIVATE Boost::boost OpenSSL::Crypto nlohmann_json::nlohmann_json)

add_executable(watcher_test client/watcher_test.cpp client/watcher.cpp client/hashing.cpp)
target_link_libraries(watcher_test PRIVATE  OpenSSL::Crypto)