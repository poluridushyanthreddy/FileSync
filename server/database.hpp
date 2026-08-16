#pragma once
#include <sqlite3.h>
#include <string>

class Database {
    public:
    Database(const std::string& db_path);
    ~Database();

    //returns the new version number after insert/update
    //returns -1 if hash matches an existing row (write isn't needed)
    int upsert_file(const::std::string& filename,const std::string& hash,
                size_t size,long modified_at);

    void delete_file(const std::string& filename);
    
    private:
    sqlite3* db;
};