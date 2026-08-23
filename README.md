# FileSync

A Dropbox-style file synchronization tool built from scratch in C++17 — covering TCP networking, binary protocols, cryptographic hashing, multithreading, and database persistence.

A client watches a local folder for changes and automatically syncs new, modified, and deleted files to a server. The server concurrently handles multiple client connections, stores files on disk, and tracks metadata (including version history) in SQLite.

## Architecture

```
Client(s)                               Server
---------                               ------
watches sync folder (polling)           listens on TCP port 9000
detects file changes (new/mod/del)      spawns worker thread per client connection
computes SHA-256 hash                   receives JSON header + raw bytes
sends JSON header + raw bytes           saves file to storage/
receives ACK                            upserts metadata into SQLite (mutex-protected)
                                        sends back ACK
```

**Wire protocol:** A single JSON header line, followed immediately by raw file bytes (for uploads).

Example upload header:
```json
{"type": "FILE_UPLOAD", "filename": "report.docx", "hash": "a3f9...", "size": 20480}
```

Example delete header (no file bytes follow):
```json
{"type": "FILE_DELETE", "filename": "report.docx"}
```

Server response:
```json
{"type": "ACK", "status": "ok", "version": 3}
```

## Features

- **Concurrent multi-client server** (`std::thread`, Boost.Asio) — accepts incoming connections and dispatches each client to a dedicated detached worker thread.
- **Thread-safe SQLite persistence** (`std::mutex`) — file metadata (filename, hash, size, modified time, version) is synchronized safely across concurrent client operations. Uploading a file with an unchanged hash is a no-op; a changed hash increments the version.
- **TCP client** (Boost.Asio) — connects, transmits file bytes or delete notifications, and processes server ACKs. Supports custom watch folders passed via CLI arguments.
- **SHA-256 hashing** (OpenSSL) — every file is hashed before upload for wire protocol integrity and local change detection.
- **Folder watcher** (`std::filesystem`, polling every 5s) — scans the local directory using a filename-to-hash map to detect new, modified, unchanged, or deleted files.
- **Delete detection** — removing a file from the watched directory triggers a `FILE_DELETE` command; the server deletes both the stored file on disk and its database entry.
- **Error handling** — handles malformed headers, dropped connections, and I/O failures gracefully without crashing the server.

## Tech Stack

| Purpose | Library / Tool |
|---|---|
| Language / Build | C++17, CMake |
| Networking | Boost.Asio (synchronous TCP) |
| Concurrency | `std::thread`, `std::mutex` |
| Hashing | OpenSSL (SHA-256) |
| Metadata Persistence | SQLite3 (C API, thread-safe) |
| Message Serialization | nlohmann/json |
| Folder Watching | `std::filesystem` (polling) |

## Project Structure

```
FileSync/
├── server/
│   ├── main.cpp          # TCP accept loop & multi-threaded client dispatch
│   ├── database.hpp/.cpp # Thread-safe SQLite wrapper (upsert, delete, versioning)
│
├── client/
│   ├── main.cpp          # Watch loop, folder CLI argument & upload/delete dispatch
│   ├── watcher.hpp/.cpp  # Folder polling & change detection (new/modified/deleted)
│   ├── hashing.hpp/.cpp  # SHA-256 via OpenSSL
│
├── storage/              # Server-side file storage (gitignored)
├── database/             # sync.db lives here (gitignored)
├── sync_folder/          # Client-side watched folder (gitignored)
└── CMakeLists.txt
```

## Building

Requires a C++17 compiler, CMake, and the following development libraries: Boost, OpenSSL, SQLite3, and nlohmann-json.

On Ubuntu / Debian:
```bash
sudo apt install build-essential cmake libboost-all-dev libssl-dev libsqlite3-dev nlohmann-json3-dev
```

Build:
```bash
mkdir -p build && cd build
cmake ..
cmake --build .
```

This produces the `server` and `client` binaries in `build/`.

## Running

### 1. Initialize the Database
```bash
mkdir -p database
sqlite3 database/sync.db "CREATE TABLE IF NOT EXISTS files (id INTEGER PRIMARY KEY AUTOINCREMENT, filename TEXT NOT NULL, hash TEXT NOT NULL, size INTEGER NOT NULL, modified_at INTEGER NOT NULL, version INTEGER NOT NULL DEFAULT 1);"
```

### 2. Start the Server
```bash
cd build
./server
```

### 3. Start Client(s)
In a separate terminal, start a client (watches `../sync_folder` by default):
```bash
cd build
./client
```

To run multiple concurrent clients watching different folders:
```bash
./client ../sync_folder2
```

Drop, edit, or delete files in the watched folders — changes are automatically detected and synced to the server. Synced files are saved to `storage/`, and metadata is tracked in `database/sync.db`:

```bash
sqlite3 database/sync.db "SELECT * FROM files;"
```