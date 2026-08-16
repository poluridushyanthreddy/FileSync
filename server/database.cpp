#include "database.hpp"
#include <iostream>
#include <stdexcept>

Database::Database(const std::string& db_path) {
    int rc = sqlite3_open(db_path.c_str(), &db);
    if (rc != SQLITE_OK) {
        throw std::runtime_error("Failed to open database: " + std::string(sqlite3_errmsg(db)));
    }
}

Database::~Database() {
    sqlite3_close(db);
}

int Database::upsert_file(const std::string& filename, const std::string& hash,
                            size_t size, long modified_at) {
    // Step 1: check if a row with this filename already exists
    const char* select_sql = "SELECT hash, version FROM files WHERE filename = ?;";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db, select_sql, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, filename.c_str(), -1, SQLITE_TRANSIENT);

    bool found = false;
    std::string existing_hash;
    int existing_version = 0;

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        found = true;
        existing_hash = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        existing_version = sqlite3_column_int(stmt, 1);
    }
    sqlite3_finalize(stmt);

    if (found && existing_hash == hash) {
        // Hash matches — nothing changed, skip the write
        return -1;
    }

    if (!found) {
        // Step 2a: insert new row
        const char* insert_sql =
            "INSERT INTO files (filename, hash, size, modified_at, version) "
            "VALUES (?, ?, ?, ?, 1);";
        sqlite3_prepare_v2(db, insert_sql, -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, filename.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, hash.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 3, static_cast<sqlite3_int64>(size));
        sqlite3_bind_int64(stmt, 4, modified_at);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        return 1;
    } else {
        // Step 2b: update existing row, bump version
        int new_version = existing_version + 1;
        const char* update_sql =
            "UPDATE files SET hash = ?, size = ?, modified_at = ?, version = ? "
            "WHERE filename = ?;";
        sqlite3_prepare_v2(db, update_sql, -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, hash.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 2, static_cast<sqlite3_int64>(size));
        sqlite3_bind_int64(stmt, 3, modified_at);
        sqlite3_bind_int(stmt, 4, new_version);
        sqlite3_bind_text(stmt, 5, filename.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        return new_version;
    }
}

void Database::delete_file(const std::string& filename) {
    const char* delete_sql = "DELETE FROM files WHERE filename = ?;";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db, delete_sql, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, filename.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}