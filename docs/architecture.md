# Project architecture

This document describes the internal architecture of Pear-to-Pear and the boundaries between CLI, workspace, metadata storage and network communication.

Pear-to-Pear can be used in two modes:

1. as a command-line application through `pear`;
2. as a reusable C++20 core for projects that need local file storage, metadata versioning and p2p synchronization.

<p align="center">
  <img src="assets/architecture.png" width="760" alt="Pear-to-Pear architecture">
</p>

## Public headers

The public headers are located under `include/pear/`.

```text
include/pear/
├── cli/
├── db/
├── demon/
├── fs/
└── net/
```

Applications that embed Pear-to-Pear should treat `include/pear/` as the integration boundary. Files under `src/` are implementation details.

## High-level layers

| Layer | Main responsibility | Typical public types |
|---|---|---|
| CLI | Parses command line arguments and maps commands to repository operations | CLI command handlers |
| Demon | Keeps a local node running in background mode | demon helpers |
| Workspace | Owns user files, `.peer` service directory and object storage | `pear::fs::Workspace` |
| Database | Stores repository metadata and local state in SQLite | `pear::db::SqliteDatabase` |
| Network node | Owns gRPC server and service implementations | `pear::net::Node` |
| Metadata service | Synchronizes devices, WAL entries and file metadata | `pear::net::MasterServiceImpl` |
| Storage service | Transfers object content between peers | `pear::net::StorageServiceImpl` |
| Remote client | Performs outgoing metadata and object requests | `pear::net::RemoteClient` |

## Local node

A local node is represented by `pear::net::Node`.

The node owns the network server and service implementations. It receives already constructed storage dependencies:

```cpp
pear::net::Node::Node(
    std::shared_ptr<pear::db::SqliteDatabase> db,
    std::shared_ptr<pear::fs::Workspace> workspace,
    bool is_master = false
);
```

Runtime control:

```cpp
void start(const std::string& listen_address, bool storage_only = false);
void stop();
bool is_running() const;
```

### Parameters

| Parameter | Meaning |
|---|---|
| `db` | Shared metadata storage used by network services |
| `workspace` | Local workspace and object storage |
| `is_master` | Enables metadata coordination service for the node |
| `listen_address` | Address used by the gRPC server, for example `127.0.0.1:50051` |
| `storage_only` | Starts only object-transfer service when metadata service is not required |

## Metadata database

The metadata layer is implemented by `pear::db::SqliteDatabase`.

The database is responsible for:

- WAL storage;
- applying remote WAL entries to local state;
- resolving file versions;
- staging local changes;
- storing known devices;
- storing object owners;
- storing local node configuration.

Important methods:

```cpp
std::vector<pear::net::WalEntryInfo> getWalEntriesSince(uint64_t last_seq_id);
void applyWalEntries(const std::vector<pear::net::WalEntryInfo>& entries);

std::optional<pear::net::FileInfo> getFileInfoByPath(const std::string& path, uint64_t version);
std::optional<std::string> getObjectHashByPath(const std::string& path);

uint64_t addWalEntry(const pear::net::WalEntryInfo& entry);
uint64_t getLastSeqId();
uint64_t getNextVersion(const std::string& path);

void stageFile(const std::string& path,
               const std::string& object_hash,
               const std::string& local_path,
               const std::string& operation = "add");

void unstageFile(const std::string& path);
std::vector<pear::db::StagedFileInfo> getStagedFiles();
void clearStaging();

uint64_t registerDevice(const std::string& address);
std::string getDeviceAddress(uint64_t device_id);

std::vector<uint64_t> getObjectOwnerDeviceIds(const std::string& object_hash);
std::vector<std::string> getObjectOwnerAddresses(const std::string& object_hash);
bool hasObjectOwner(const std::string& object_hash, uint64_t device_id);

void setMasterAddress(const std::string& address);
std::string getMasterAddress();

void setDeviceId(uint64_t id);
uint64_t getDeviceId();
```

## Repository state model

Pear-to-Pear separates file content from metadata.

Metadata answers questions such as:

- which files exist;
- which version is current;
- which object hash belongs to a version;
- which devices own a copy of the object;
- which operations have already been applied.

File content is stored separately in object storage. Objects are addressed by content hash.

## WAL and synchronization invariant

The WAL is the ordered operation log for repository metadata.

Each metadata-changing operation should be represented as a WAL entry. Nodes synchronize by exchanging WAL entries, not by exchanging complete database snapshots.

Required invariant:

```text
if two nodes have applied the same ordered WAL prefix,
their metadata state must be equivalent
```

This is the main reason why operations are stored in WAL before being applied to the local repository state.

## Object ownership invariant

`OBJECT_OWNERS` stores which devices can provide object content.

A node may know metadata for a file version without having the corresponding object locally. In that case, the file content must be pulled from an owner device.

Required invariant:

```text
metadata synchronization must not imply local content availability
```

Use `pear pull` or storage-layer requests to fetch object content explicitly.

## Workspace layout

A workspace contains user files and the `.peer` service directory.

```text
workspace/
├── user files...
└── .peer/
    ├── meta
    ├── obj/
    └── config
```

| Path | Purpose |
|---|---|
| `.peer/meta` | SQLite metadata database |
| `.peer/obj/` | Content-addressed object storage |
| `.peer/config` | Local node configuration |

## Extension boundaries

Stable extension points are:

- CLI can be replaced by another UI layer;
- transport can be replaced by another implementation with the same operation contract;
- workspace and database layers can be reused by another application;
- storage transfer can be routed through another network mechanism.

Do not couple application-specific logic directly to SQLite tables or gRPC-generated classes. Prefer using the public C++ types in `include/pear/`.
