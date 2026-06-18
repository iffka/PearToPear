# Transport layer

This document describes the transport boundary of Pear-to-Pear and how to replace or extend network communication.

The main design rule:

```text
repository logic must not depend on a concrete wire protocol
```

The repository core operates on metadata entries, object hashes, device ids and file paths. The transport layer is responsible only for delivering those operations between nodes.

## Current implementation

The default transport implementation uses:

| Technology | Role |
|---|---|
| gRPC | RPC transport |
| Protobuf | Message schema |
| `MasterServiceImpl` | Metadata service implementation |
| `StorageServiceImpl` | Object transfer service implementation |
| `RemoteClient` | Outgoing client-side network API |
| `Node` | Server owner and lifecycle controller |

## Existing client-side transport boundary

The current outgoing API is represented by `pear::net::RemoteClient`.

It provides four logical operations:

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

A custom transport must preserve the semantics of these operations.

## Recommended adapter interface

For a custom transport, introduce an adapter with the same operation contract.

```cpp
class IRemoteTransport {
public:
    virtual ~IRemoteTransport() = default;

    virtual uint64_t registerDevice(const std::string& coordinator_address,
                                    const std::string& local_address) = 0;

    virtual std::vector<pear::net::WalEntryInfo> updateDB(const std::string& coordinator_address,
                                                         uint64_t last_seq_id,
                                                         uint64_t device_id) = 0;

    virtual bool pushWAL(const std::string& coordinator_address,
                         uint64_t device_id,
                         const std::vector<pear::net::WalEntryInfo>& entries,
                         std::vector<uint64_t>& out_assigned_seq_ids) = 0;

    virtual void downloadFile(const std::string& owner_address,
                              const std::string& object_hash,
                              uint64_t requester_device_id,
                              const std::string& destination_path) = 0;
};
```

Then implement a concrete transport.

```cpp
class GrpcRemoteTransport final : public IRemoteTransport {
public:
    uint64_t registerDevice(const std::string& coordinator_address,
                            const std::string& local_address) override;

    std::vector<pear::net::WalEntryInfo> updateDB(const std::string& coordinator_address,
                                                 uint64_t last_seq_id,
                                                 uint64_t device_id) override;

    bool pushWAL(const std::string& coordinator_address,
                 uint64_t device_id,
                 const std::vector<pear::net::WalEntryInfo>& entries,
                 std::vector<uint64_t>& out_assigned_seq_ids) override;

    void downloadFile(const std::string& owner_address,
                      const std::string& object_hash,
                      uint64_t requester_device_id,
                      const std::string& destination_path) override;
};
```

For a relay-based implementation:

```cpp
class RelayRemoteTransport final : public IRemoteTransport {
public:
    explicit RelayRemoteTransport(std::string relay_address);

    uint64_t registerDevice(const std::string& coordinator_address,
                            const std::string& local_address) override;

    std::vector<pear::net::WalEntryInfo> updateDB(const std::string& coordinator_address,
                                                 uint64_t last_seq_id,
                                                 uint64_t device_id) override;

    bool pushWAL(const std::string& coordinator_address,
                 uint64_t device_id,
                 const std::vector<pear::net::WalEntryInfo>& entries,
                 std::vector<uint64_t>& out_assigned_seq_ids) override;

    void downloadFile(const std::string& owner_address,
                      const std::string& object_hash,
                      uint64_t requester_device_id,
                      const std::string& destination_path) override;

private:
    std::string relay_address_;
};
```

## Operation semantics

### `registerDevice`

Registers a node address and returns its device id.

Required behavior:

- input address must identify the registering node;
- returned device id must be stable for the registered device;
- duplicate registration policy must be deterministic.

### `updateDB`

Returns WAL entries after `last_seq_id`.

Required behavior:

- entries must be ordered by sequence id;
- entries must not skip committed sequence ids;
- returned entries must be valid for `device_id`;
- empty result means the node is already up to date.

### `pushWAL`

Publishes local WAL entries.

Required behavior:

- all entries must be accepted atomically or rejected;
- assigned sequence ids must match committed order;
- the output vector must contain ids corresponding to input entries;
- returning `true` means the caller may clear local staging;
- returning `false` means the caller must keep local staging.

### `downloadFile`

Downloads object content by hash.

Required behavior:

- `object_hash` identifies the requested content;
- downloaded bytes must match the requested hash;
- `destination_path` must be written atomically where possible;
- failure must not leave a corrupted object marked as valid.

## Server-side services

### `MasterServiceImpl`

`MasterServiceImpl` is responsible for metadata-level RPC operations.

It should use `SqliteDatabase` for:

- device registration;
- WAL lookup;
- WAL insertion;
- applying operations to metadata state;
- object owner tracking.

The service must not transfer full file content as part of metadata synchronization.

### `StorageServiceImpl`

`StorageServiceImpl` is responsible for object-level transfer.

It should use `Workspace` for:

- resolving object paths;
- reading object content;
- validating requested object availability;
- serving file bytes to requesting peers.

The service must not modify metadata ordering.

## Test transport

A test transport can implement the same contract without real sockets.

Example use cases:

- deterministic unit tests;
- multi-peer tests in one process;
- failure injection;
- offline peer simulation;
- WAL ordering tests.

Recommended structure:

```cpp
class InMemoryRemoteTransport final : public IRemoteTransport {
public:
    explicit InMemoryRemoteTransport(TestNetwork& network);

    uint64_t registerDevice(const std::string& coordinator_address,
                            const std::string& local_address) override;

    std::vector<pear::net::WalEntryInfo> updateDB(const std::string& coordinator_address,
                                                 uint64_t last_seq_id,
                                                 uint64_t device_id) override;

    bool pushWAL(const std::string& coordinator_address,
                 uint64_t device_id,
                 const std::vector<pear::net::WalEntryInfo>& entries,
                 std::vector<uint64_t>& out_assigned_seq_ids) override;

    void downloadFile(const std::string& owner_address,
                      const std::string& object_hash,
                      uint64_t requester_device_id,
                      const std::string& destination_path) override;

private:
    TestNetwork& network_;
};
```

## What can be replaced

| Part | Can be replaced | Notes |
|---|---:|---|
| gRPC client calls | Yes | Implement transport adapter |
| gRPC services | Yes | Preserve metadata/object operation semantics |
| Protobuf schema | Yes | Keep equivalent domain model |
| Peer discovery | Yes | Address resolution can be custom |
| Object routing | Yes | Object identity must remain hash-based |
| SQLite metadata storage | Not recommended | Requires preserving WAL/state invariants |
| Workspace object storage | Not recommended | Requires preserving object hash semantics |

## Transport invariants

Any transport implementation must preserve these invariants:

```text
1. WAL entries are delivered in a deterministic order.
2. A successful push assigns stable sequence ids.
3. Metadata synchronization does not imply object availability.
4. Object download is addressed by object hash.
5. A node may serve only objects it actually owns.
6. Failed object downloads do not produce valid local objects.
7. Retrying update or download is safe.
```

## Extension example

An example external transport scenario is available here:

- [PearToPearRelay](https://github.com/p2pSquad/PearToPearRelay)
