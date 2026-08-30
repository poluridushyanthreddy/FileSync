# FileSync

A Dropbox-style file synchronization tool built from scratch in C++17 — covering TCP networking, binary protocols, cryptographic hashing, multithreading, SQLite persistence, and version-based conflict resolution.

A client watches a local folder for changes and automatically syncs new, modified, and deleted files to a server. The server concurrently handles multiple client connections, stores files on disk, tracks file versions in SQLite, and resolves upload conflicts automatically.

## Architecture

```
Client(s)                                    Server
---------                                    ------
watches sync folder (polling every 5s)       listens on TCP port 9000
detects file changes (new/mod/del)           spawns detached worker thread per client
tracks local FileState (hash + base_version) receives JSON header + raw bytes
sends JSON header (with base_version)        checks DB for current_version:
                                              ├─ If conflict (base != 0 && base != current):
                                              │   saves to storage/<file>.conflict
                                              │   sends ACK (status="conflict")
                                              └─ If valid / up-to-date:
                                                  saves to storage/<file>
                                                  upserts SQLite metadata & bumps version
                                                  sends ACK (status="ok", new version)
receives ACK & updates local version
```

### Wire Protocol

A single newline-delimited JSON header followed immediately by raw file bytes (for uploads).

**1. File Upload Header:**
```json
{"type": "FILE_UPLOAD", "filename": "report.docx", "hash": "a3f9...", "size": 20480, "base_version": 1}
```

**2. File Delete Header:** (No raw bytes follow)
```json
{"type": "FILE_DELETE", "filename": "report.docx"}
```

**3. Server Responses (ACK):**
- **Success:**
  ```json
  {"type": "ACK", "status": "ok", "version": 2}
  ```
- **Conflict Detected:**
  ```json
  {"type": "ACK", "status": "conflict", "current_version": 2, "conflict_file": "report.docx.conflict"}
  ```

## Features

- **Concurrent Multi-Client Server** (`std::thread`, Boost.Asio) — Accepts incoming connections and dispatches each client connection to a dedicated detached worker thread.
- **Version-Based Conflict Resolution** — Uses optimistic versioning. Clients transmit their `base_version` on upload; if the server has a newer or conflicting version (`base_version != 0 && base_version != current_version`), the server rejects overwriting the canonical file, saves the incoming payload as `<filename>.conflict`, and returns a conflict ACK.
- **Thread-Safe SQLite Persistence** (`std::mutex`) — File metadata (`filename`, `hash`, `size`, `modified_at`, `version`) is protected by a mutex across worker threads. Unchanged file uploads are no-ops; modifications bump the version number.
- **SHA-256 Hashing & Change Detection** (OpenSSL, `std::filesystem`) — Client polls directory every 5s, computing SHA-256 checksums to identify new, modified, or deleted files against its known state.
- **Delete Propagation** — Deleting a file locally sends a `FILE_DELETE` command, prompting the server to remove the stored file from disk and its SQLite record.
- **Resilient Error Handling** — Safely handles malformed headers, socket read/write errors, and unexpected disconnections without crashing the server.

## Tech Stack

| Purpose | Library / Tool |
|---|---|
| Language / Build | C++17, CMake |
| Networking | Boost.Asio (synchronous TCP) |
| Concurrency | `std::thread`, `std::mutex` |
| Hashing | OpenSSL (SHA-256) |
| Metadata Persistence | SQLite3 (C API, thread-safe) |
| Serialization | nlohmann/json |
| Folder Watching | `std::filesystem` (polling) |

## Project Structure

```
FileSync/
├── server/
│   ├── main.cpp          # TCP accept loop, worker thread dispatch & conflict handling
│   ├── database.hpp/.cpp # Thread-safe SQLite wrapper (upsert, delete, version lookup)
│
├── client/
│   ├── main.cpp          # Watch loop, folder CLI argument & upload/delete dispatch
│   ├── watcher.hpp/.cpp  # Folder polling, FileState tracking & change detection
│   ├── hashing.hpp/.cpp  # SHA-256 via OpenSSL
│
├── storage/              # Server-side file storage & .conflict files (gitignored)
├── database/             # sync.db SQLite database (gitignored)
├── sync_folder/          # Default client watch folder (gitignored)
└── CMakeLists.txt
```

## Building

Requires a C++17 compiler, CMake, and development libraries for Boost, OpenSSL, SQLite3, and nlohmann-json.

**Ubuntu / Debian:**
```bash
sudo apt install build-essential cmake libboost-all-dev libssl-dev libsqlite3-dev nlohmann-json3-dev
```

**Build:**
```bash
mkdir -p build && cd build
cmake ..
cmake --build .
```

Generates `server`, `client`, and `watcher_test` binaries in `build/`.

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
Default client (watches `../sync_folder`):
```bash
cd build
./client
```

Run multiple clients watching distinct directories:
```bash
./client ../sync_folder_client1
./client ../sync_folder_client2
```

### 4. Conflict Resolution Flow
1. Client 1 uploads version 1 of `notes.txt`.
2. Client 2 (unaware or outdated) attempts to upload `notes.txt` with `base_version = 0` (or stale version) when the server is already at a newer version.
3. Server detects the version mismatch, keeps the canonical `storage/notes.txt` intact, writes the incoming data to `storage/notes.txt.conflict`, and responds with `{"status": "conflict", ...}`.