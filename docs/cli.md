# CLI reference

This document describes the command-line interface provided by the `pear` binary.

The CLI is a thin user-facing layer over the repository core. Commands usually perform one of the following operations:

- initialize or remove workspace metadata;
- start or stop a local node;
- stage local file changes;
- publish metadata operations;
- synchronize metadata;
- fetch object content;
- inspect repository state.

## Global syntax

```text
pear <command> [options] [arguments]
```

Commands must be executed either with an explicit workspace path, when the command accepts it, or from inside an initialized workspace.

## Repository lifecycle

### `pear init`

```text
pear init <workspace_path>
```

Initializes a Pear-to-Pear workspace.

Effects:

- creates the workspace directory if needed;
- creates the `.peer` service directory;
- initializes local metadata storage;
- prepares object storage;
- writes local configuration.

Arguments:

| Argument | Description |
|---|---|
| `<workspace_path>` | Path to the workspace root |

### `pear deinit`

```text
pear deinit
```

Removes Pear-to-Pear service metadata from the current workspace.

This command affects `.peer` metadata. User files are not intended to be deleted by this command.

## Node connection

### `pear connect --main`

```text
pear connect --main --listen <ip:port>
```

Starts the current workspace as the main node.

Options:

| Option | Description |
|---|---|
| `--main` | Start node in metadata-coordination mode |
| `--listen <ip:port>` | Address used by the local gRPC server |

Example:

```bash
pear connect --main --listen 127.0.0.1:50051
```

### `pear connect --gu`

```text
pear connect --gu <ip:port> --listen <ip:port>
```

Connects the current node to an existing main node.

Options:

| Option | Description |
|---|---|
| `--gu <ip:port>` | Address of the main node |
| `--listen <ip:port>` | Address used by the local node |

Example:

```bash
pear connect --gu 127.0.0.1:50051 --listen 127.0.0.1:50052
```

### `pear disconnect`

```text
pear disconnect
```

Stops the local node connection and shuts down the background process for the workspace.

## Staging

Staging stores local changes before they are published to the shared repository state.

### `pear add`

```text
pear add <path>...
```

Stages one or more files or directories.

Arguments:

| Argument | Description |
|---|---|
| `<path>...` | Files or directories to stage |

### `pear add --all`

```text
pear add --all
```

Stages all detected local changes.

### `pear add --readonly`

```text
pear add --readonly <path>...
```

Stages files in readonly mode.

Readonly mode is useful when the repository should own the file content as an object without keeping an additional full working copy.

### `pear unstage`

```text
pear unstage <path>...
```

Removes selected paths from staging.

### `pear unstage --all`

```text
pear unstage --all
```

Clears all staged changes.

## Readonly mode

### `pear readonly`

```text
pear readonly <path>...
```

Stages conversion of tracked files to readonly mode.

### `pear readonly --off`

```text
pear readonly --off <path>...
```

Stages disabling readonly mode for tracked files.

## Publishing and synchronization

### `pear push`

```text
pear push
```

Publishes staged local changes.

Typical effects:

- creates object entries for staged file content;
- creates WAL records for metadata changes;
- sends metadata operations to the main node;
- clears staging after successful publication.

### `pear update`

```text
pear update
```

Synchronizes local metadata state.

Typical effects:

- reads local last known WAL sequence;
- requests missing WAL entries;
- stores received entries locally;
- applies entries to the local metadata database.

### `pear pull`

```text
pear pull <file-or-dir>...
```

Downloads content for selected files or directories.

Arguments:

| Argument | Description |
|---|---|
| `<file-or-dir>...` | Repository paths to materialize locally |

### `pear pull --no-share`

```text
pear pull --no-share <file-or-dir>...
```

Downloads content without registering the current node as a further content owner.

Use this mode when the node should consume data but should not become a provider for downloaded objects.

## Cleanup

### `pear cleanup`

```text
pear cleanup <keep_versions> <path>...
```

Removes old or unused local objects according to the selected retention policy.

Arguments:

| Argument | Description |
|---|---|
| `<keep_versions>` | Number of recent versions to keep |
| `<path>...` | Files or directories to clean |

### `pear cleanup --all`

```text
pear cleanup <keep_versions> --all
```

Runs cleanup for all tracked files.

## Inspection

### `pear status`

```text
pear status
pear status --json
```

Shows local repository state, including staged changes.

`--json` emits machine-readable output.

### `pear ls`

```text
pear ls
pear ls --json
```

Shows files known in the repository metadata.

`--json` emits machine-readable output.

### `pear log`

```text
pear log
pear log --tail <n>
```

Shows WAL / operation history.

Options:

| Option | Description |
|---|---|
| `--tail <n>` | Print only the last `n` records |

## Typical workflow

Main node:

```bash
mkdir -p /tmp/pear-main
pear init /tmp/pear-main

cd /tmp/pear-main
pear connect --main --listen 127.0.0.1:50051

printf 'hello\n' > note.txt
pear add note.txt
pear push
```

Peer node:

```bash
mkdir -p /tmp/pear-peer
pear init /tmp/pear-peer

cd /tmp/pear-peer
pear connect --gu 127.0.0.1:50051 --listen 127.0.0.1:50052

pear update
pear pull note.txt
cat note.txt
```
