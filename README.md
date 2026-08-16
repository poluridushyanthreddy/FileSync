# FileSync

A Dropbox-style file synchronization tool built from scratch in C++17 — a hands-on systems programming project covering TCP networking, binary protocols, cryptographic hashing, and database persistence.

A client watches a local folder for changes and automatically syncs new, modified, and deleted files to a server, which stores the files on disk and tracks their metadata (including version history) in SQLite.

## Demo

*(Coming soon — terminal recording / GIF walkthrough of the client watching a folder and syncing to the server in real time.)*

## Architecture

```
Client                              Server
------                              ------
watches sync_folder/                listens on TCP port 9000
detects file changes (polling)      receives {header + bytes}
computes SHA-256 hash               saves file to storage/
sends JSON header + raw bytes       upserts metadata into SQLite
receives ACK                        sends back ACK
```

**Wire protocol:** a single JSON header line, followed immediately by the raw file bytes (for uploads). No chunking or streaming yet — the whole file is loaded into memory and sent in one shot.

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

## What's implemented

- **TCP server** (Boost.Asio, synchronous) — accepts connections in a loop, parses the JSON header, reads the exact number of file bytes, and writes the file to `storage/`
- **TCP client** (Boost.Asio, synchronous) — connects, sends a file with its computed hash, and reads back the server's ACK
- **SHA-256 hashing** (OpenSSL) — every file is hashed before upload; the hash is used both for the wire protocol and for local change detection
- **SQLite persistence** — file metadata (filename, hash, size, modified time, version) is stored in a `files` table. Uploading a file with an unchanged hash is a no-op; a changed hash bumps the row's `version`
- **Folder watcher** (`std::filesystem`, polling every 5s) — tracks a local map of filename → last-known-hash to classify each file on every scan as new, modified, unchanged, or deleted
- **Delete detection** — a file removed from the watched folder triggers a `FILE_DELETE` message; the server removes both the DB row and the stored copy
- **Basic error handling** — malformed headers, dropped connections, and read/write failures are caught and logged; the server keeps running and accepting new connections rather than crashing

## What's explicitly out of scope (for now)

This is an MVP. The following are deliberately deferred to keep the initial build focused and shippable:

- **Multiple simultaneous clients** — the server currently handles one connection at a time
- **Authentication** — no user accounts or tokens yet
- **Conflict resolution** — no conflict-named copies or merge logic
- **Chunked / delta / compressed / encrypted transfer** — files are sent whole, in the clear
- **Rename detection** — a rename is currently treated as a delete + a new file
- **Event-driven watching** — the watcher polls every 5 seconds rather than using `inotify`
- **A dashboard or GUI** — everything is CLI/terminal only

These are the natural next milestones, with multi-client support in particular expected to meaningfully strengthen the project (it's the point where the server needs real concurrency handling).

## Tech stack

| Purpose              | Library / Tool          |
|-----------------------|-------------------------|
| Language / build      | C++17, CMake            |
| Networking             | Boost.Asio (synchronous TCP) |
| Hashing                 | OpenSSL (SHA-256)       |
| Metadata persistence    | SQLite3 (raw C API)     |
| Message serialization   | nlohmann/json           |
| Folder watching          | `std::filesystem` (polling) |

## Project structure

```
FileSync/
├── server/
│   ├── main.cpp          # TCP accept loop, protocol dispatch (upload/delete)
│   ├── database.hpp/.cpp # SQLite wrapper — upsert with version bumping, delete
│
├── client/
│   ├── main.cpp           # watch loop, upload/delete dispatch
│   ├── watcher.hpp/.cpp   # folder polling, change detection (new/modified/deleted)
│   ├── hashing.hpp/.cpp   # SHA-256 via OpenSSL
│
├── storage/                # server-side file storage (gitignored)
├── database/                # sync.db lives here (gitignored)
├── sync_folder/               # client-side watched folder (gitignored)
└── CMakeLists.txt
```

## Building

Requires a C++17 compiler, CMake, and the following dev libraries: Boost, OpenSSL, SQLite3, and nlohmann-json.

On Ubuntu/WSL:
```bash
sudo apt install build-essential cmake libboost-all-dev libssl-dev libsqlite3-dev nlohmann-json3-dev
```

Then build:
```bash
mkdir -p build && cd build
cmake ..
cmake --build .
```

This produces two executables in `build/`: `server` and `client`.

## Running it

First, set up the database (one-time):
```bash
mkdir -p database
sqlite3 database/sync.db < schema.sql   # or run the CREATE TABLE statement directly
```

Then, in one terminal, start the server:
```bash
cd build
./server
```

In another terminal, start the client (it watches `../sync_folder` by default):
```bash
cd build
./client
```

Now drop, edit, or delete files in `sync_folder/` — the client will detect the change within 5 seconds and sync it to the server automatically. Synced files land in `storage/`, and their metadata (including version history) is queryable in `database/sync.db`:

```bash
sqlite3 database/sync.db "SELECT * FROM files;"
```

## Learning notes

This project was built incrementally, stage by stage, with each piece (sockets, hashing, database, watcher) written by hand and tested in isolation before being wired together — starting with zero prior experience in Boost.Asio, SQLite's C API, or OpenSSL.