# Using Pear-to-Pear as a library

Pear-to-Pear is primarily shipped as the `pear` CLI tool, but its internal structure is suitable for reuse in custom C++20 p2p applications.

This document describes the intended library-level integration model.

## Include layout

Public headers are located under:

```text
include/pear/
```

Main include groups:

```text
include/pear/
├── db/      # metadata database
├── fs/      # workspace and object storage
├── net/     # node, services and remote client
├── cli/     # default CLI integration
└── demon/   # background process helpers
```

Applications should include headers from `include/pear/` and avoid depending on implementation files from `src/`.

## Core objects

### `pear::db::SqliteDatabase`

`SqliteDatabase` owns the local metadata database.

Typical responsibilities:

- create and maintain repository tables;
- store WAL entries;
- apply WAL entries to repository state;
- stage local file operations;
- resolve file versions;
- map object hashes to owner devices;
- store local node id and main-node address.

Typical construction:

```cpp
auto db = std::make_shared<pear::db::SqliteDatabase>(workspace_root / ".peer" / "meta");
```

### `pear::fs::Workspace`

`Workspace` owns the file-system side of the repository.

Typical responsibilities:

- manage workspace root;
- manage `.peer` directory;
- store object files under `.peer/obj`;
- prepare file content before staging;
- materialize pulled objects back into the workspace.

Typical construction:

```cpp
auto workspace = std::make_shared<pear::fs::Workspace>(workspace_root);
```

### `pear::net::Node`

`Node` owns the local network server and service implementations.

```cpp
pear::net::Node node(db, workspace, is_master);
node.start("127.0.0.1:50051");
node.stop();
```

Constructor contract:

```cpp
Node(std::shared_ptr<pear::db::SqliteDatabase> db,
     std::shared_ptr<pear::fs::Workspace> workspace,
     bool is_master = false);
```

Runtime API:

```cpp
void start(const std::string& listen_address, bool storage_only = false);
void stop();
bool is_running() const;
```

Parameters:

| Parameter | Description |
|---|---|
| `db` | Metadata storage shared by services |
| `workspace` | Workspace and object storage |
| `is_master` | Enables metadata-coordination behavior |
| `listen_address` | Address for the local server |
| `storage_only` | Starts only storage service when metadata service is not needed |

### `pear::net::RemoteClient`

`RemoteClient` is the current outgoing network boundary.

It exposes metadata and object operations used by higher-level synchronization logic.

Expected operation set:

```cpp
static uint64_t RegisterDevice(const std::string& gu_address,
                               const std::string& my_address);

static std::vector<pear::net::WalEntryInfo> UpdateDB(const std::string& gu_address,
                                                     uint64_t last_seq_id,
                                                     uint64_t device_id);

static bool PushWAL(const std::string& gu_address,
                    uint64_t device_id,
                    const std::vector<pear::net::WalEntryInfo>& entries,
                    std::vector<uint64_t>& out_assigned_seq_ids);

static void DownloadFile(const std::string& vu_address,
                         const std::string& object_hash,
                         uint64_t requester_device_id,
                         const std::string& destination_path);
```

## Minimal embedding flow

A custom application usually needs the same objects as the CLI:

```cpp
#include <memory>
#include <filesystem>

#include <pear/db/sqlite_database.hpp>
#include <pear/fs/workspace.hpp>
#include <pear/net/node.hpp>

int main() {
    std::filesystem::path workspace_root = "/tmp/pear-app";

    auto db = std::make_shared<pear::db::SqliteDatabase>(workspace_root / ".peer" / "meta");
    auto workspace = std::make_shared<pear::fs::Workspace>(workspace_root);

    pear::net::Node node(db, workspace, false);
    node.start("127.0.0.1:50052");

    // Application-specific logic here.

    node.stop();
}
```

## Reusing repository logic

A host application can reuse Pear-to-Pear at several levels.

### CLI-level reuse

Use the `pear` binary directly.

Best for:

- shell scripts;
- manual workflows;
- demos;
- integration tests.

### Node-level reuse

Construct `SqliteDatabase`, `Workspace` and `Node` directly.

Best for:

- custom local applications;
- GUI wrappers;
- long-running services;
- test scenarios with multiple local nodes.

### Transport-level reuse

Keep workspace and database logic, but replace the network transport.

Best for:

- relay-based p2p;
- custom discovery;
- custom routing;
- non-gRPC environments;
- tests without real sockets.

See [Transport layer](transport.md).

## Operation model

Pear-to-Pear operations are split into metadata operations and content operations.

### Metadata operations

Metadata operations modify repository state and are stored in WAL.

Examples:

- file added;
- file updated;
- file deleted;
- object owner registered;
- readonly mode changed.

Metadata operations must be deterministic: applying the same ordered WAL prefix must produce the same metadata state.

### Content operations

Content operations transfer object files.

Examples:

- download object by hash;
- serve object to another peer;
- materialize object as a workspace file.

Content transfer is intentionally separated from metadata synchronization. A node can know about a file version before it has downloaded the corresponding object.

## Error handling expectations

Library users should treat network and file-system operations as fallible.

Recommended rules:

- check whether a node is running before issuing network operations;
- do not assume that every object owner is online;
- handle missing objects separately from missing metadata;
- apply WAL entries only once;
- do not clear staging before successful publication;
- keep object writes atomic where possible.

## Threading model

`pear::net::Node::start()` starts the server side of the local node. The node owns its server thread internally.

Recommended rules:

- call `stop()` before destroying shared database or workspace objects;
- do not destroy `SqliteDatabase` while services can still use it;
- do not destroy `Workspace` while storage requests can still use it;
- serialize write-heavy database operations unless the database layer explicitly provides stronger guarantees.

## Build integration

The project is CMake-based. A downstream project can either:

1. build Pear-to-Pear as part of the same source tree;
2. add it as a submodule;
3. link against produced targets after installing/exporting them.

Recommended future CMake layout for library consumers:

```cmake
add_subdirectory(external/PearToPear)

target_link_libraries(my_app PRIVATE pear_core)
```

If exported CMake targets are not available in the current tree, use the repository as a source dependency and link the same internal targets used by the `pear` binary.
