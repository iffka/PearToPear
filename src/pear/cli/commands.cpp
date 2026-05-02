#include <pear/cli/commands.hpp>

#include <pear/db/sqlite_database.hpp>
#include <pear/fs/workspace.hpp>
#include <pear/demon/demon.hpp>
#include <pear/net/remote_client.hpp>
#include <pear/fs/hash.hpp>

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <chrono>
#include <thread>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>

namespace {

constexpr const char* Grusha = "🍐 ";

std::filesystem::path get_database_path(const pear::storage::Workspace& workspace) {
    return workspace.get_meta_dir() / "peer.db";
}

void sync_with_master(bool verbose) {
    namespace fs = std::filesystem;

    pear::storage::Workspace workspace = pear::storage::Workspace::discover();
    pear::db::SqliteDatabase database(get_database_path(workspace));

    const std::string master_address = database.getMasterAddress();
    const uint64_t device_id = database.getDeviceId();

    if (master_address.empty()) {
        throw std::runtime_error("not connected: master address is empty");
    }
    if (device_id == 0) {
        throw std::runtime_error("not connected: device id is unknown");
    }

    const uint64_t last_seq_id = database.getLastSeqId();
    const auto wal_entries = pear::net::RemoteClient::UpdateDB(master_address, last_seq_id, device_id);

    if (!wal_entries.empty()) {
        database.applyWalEntries(wal_entries);
    }

    const auto tracked_files = database.getAllFiles();

    std::unordered_set<std::string> desired_empty_paths;

    for (const auto& file : tracked_files) {
        const fs::path workspace_file_path = workspace.get_root() / file.path;

        if (!fs::exists(workspace_file_path)) {
            desired_empty_paths.insert(file.path);
        }
    }

    fs::recursive_directory_iterator iterator(workspace.get_root());
    fs::recursive_directory_iterator end;

    while (iterator != end) {
        if (iterator->path() == workspace.get_peer_dir()) {
            iterator.disable_recursion_pending();
            ++iterator;
            continue;
        }

        if (!iterator->is_regular_file() || iterator->path().extension() != ".empty") {
            ++iterator;
            continue;
        }

        const std::string empty_relative_path = workspace.get_relative_path(iterator->path()).generic_string();
        const std::string file_relative_path = empty_relative_path.substr(
            0,
            empty_relative_path.size() - std::string(".empty").size()
        );

        if (!desired_empty_paths.contains(file_relative_path)) {
            fs::remove(iterator->path());
        }

        ++iterator;
    }

    std::vector<std::string> empty_paths(desired_empty_paths.begin(), desired_empty_paths.end());
    std::sort(empty_paths.begin(), empty_paths.end());

    workspace.create_all_empty_files(empty_paths);

    if (!verbose) {
        return;
    }

    if (wal_entries.empty()) {
        std::cout << Grusha << "already up to date\n";
    } else {
        std::cout << Grusha << "applied " << wal_entries.size() << " wal entries\n";
    }
}

struct StatusInfo {
    std::string master_address;
    uint64_t device_id = 0;

    std::vector<pear::db::StagedFileInfo> staged_files;
    std::vector<std::string> staged_paths;
    std::vector<std::string> modified_paths;
    std::vector<std::string> modified_after_staging_paths;
    std::vector<std::string> missing_paths;
    std::vector<std::string> untracked_entries;
};

std::unordered_map<std::string, pear::db::StagedFileInfo> make_staged_map(const std::vector<pear::db::StagedFileInfo>& staged_files) {
    std::unordered_map<std::string, pear::db::StagedFileInfo> staged_by_path;

    for (const auto& file : staged_files) {
        staged_by_path[file.path] = file;
    }

    return staged_by_path;
}

std::unordered_map<std::string, pear::net::FileUpdateInfo> make_tracked_map(const std::vector<pear::net::FileUpdateInfo>& tracked_files) {
    std::unordered_map<std::string, pear::net::FileUpdateInfo> tracked_by_path;

    for (const auto& file : tracked_files) {
        tracked_by_path[file.path] = file;
    }

    return tracked_by_path;
}

std::unordered_map<std::string, std::string> collect_local_hashes(const pear::storage::Workspace& workspace) {
    std::unordered_map<std::string, std::string> local_hash_by_path;

    for (const auto& file_path : workspace.collect_files(workspace.get_root())) {
        if (file_path.extension() == ".empty") {
            continue;
        }

        const std::string relative_path = workspace.get_relative_path(file_path).generic_string();
        local_hash_by_path[relative_path] = pear::storage::get_file_hash(file_path);
    }

    return local_hash_by_path;
}

std::vector<std::string> find_untracked_paths(
    const std::unordered_map<std::string, std::string>& local_hash_by_path,
    const std::unordered_map<std::string, pear::db::StagedFileInfo>& staged_by_path,
    const std::unordered_map<std::string, pear::net::FileUpdateInfo>& tracked_by_path
) {
    std::vector<std::string> untracked_paths;

    for (const auto& [path, object_hash] : local_hash_by_path) {
        if (staged_by_path.find(path) == staged_by_path.end() && tracked_by_path.find(path) == tracked_by_path.end()) {
            untracked_paths.push_back(path);
        }
    }

    std::sort(untracked_paths.begin(), untracked_paths.end());
    return untracked_paths;
}

bool is_fully_untracked_directory(
    const pear::storage::Workspace& workspace,
    const std::filesystem::path& relative_dir,
    const std::unordered_set<std::string>& untracked_path_set
) {
    namespace fs = std::filesystem;

    const fs::path directory_path = workspace.get_root() / relative_dir;

    if (!fs::exists(directory_path) || !fs::is_directory(directory_path)) {
        return false;
    }

    bool has_user_files = false;

    for (const auto& file_path : workspace.collect_files(directory_path)) {
        if (file_path.extension() == ".empty") {
            continue;
        }

        has_user_files = true;

        const std::string relative_path = workspace.get_relative_path(file_path).generic_string();
        if (untracked_path_set.find(relative_path) == untracked_path_set.end()) {
            return false;
        }
    }

    return has_user_files;
}

std::vector<std::string> compress_untracked_paths(const pear::storage::Workspace& workspace, const std::vector<std::string>& untracked_paths) {
    namespace fs = std::filesystem;

    std::set<std::string> compressed_entries;
    std::unordered_set<std::string> untracked_path_set(untracked_paths.begin(), untracked_paths.end());

    for (const auto& path : untracked_paths) {
        fs::path best_directory;
        fs::path parent = fs::path(path).parent_path();

        while (!parent.empty()) {
            if (is_fully_untracked_directory(workspace, parent, untracked_path_set)) {
                best_directory = parent;
            }

            parent = parent.parent_path();
        }

        if (best_directory.empty()) {
            compressed_entries.insert(path);
        } else {
            compressed_entries.insert(best_directory.generic_string() + "/");
        }
    }

    return std::vector<std::string>(compressed_entries.begin(), compressed_entries.end());
}

StatusInfo collect_status_info(const pear::storage::Workspace& workspace, pear::db::SqliteDatabase& database) {
    namespace fs = std::filesystem;

    StatusInfo status;
    status.master_address = database.getMasterAddress();
    status.device_id = database.getDeviceId();
    status.staged_files = database.getStagedFiles();

    const auto tracked_files = database.getAllFiles();
    const auto staged_by_path = make_staged_map(status.staged_files);
    const auto tracked_by_path = make_tracked_map(tracked_files);
    const auto local_hash_by_path = collect_local_hashes(workspace);

    for (const auto& file : status.staged_files) {
        status.staged_paths.push_back(file.path);

        auto local_hash = local_hash_by_path.find(file.path);
        if (local_hash == local_hash_by_path.end()) {
            status.missing_paths.push_back(file.path);
        } else if (local_hash->second != file.object_hash) {
            status.modified_after_staging_paths.push_back(file.path);
        }
    }

    for (const auto& file : tracked_files) {
        if (staged_by_path.find(file.path) != staged_by_path.end()) {
            continue;
        }

        auto local_hash = local_hash_by_path.find(file.path);
        if (local_hash != local_hash_by_path.end() && local_hash->second != file.object_hash) {
            status.modified_paths.push_back(file.path);
            continue;
        }

        const fs::path empty_path = workspace.get_root() / (file.path + ".empty");
        if (local_hash == local_hash_by_path.end() && !fs::exists(empty_path)) {
            status.missing_paths.push_back(file.path);
        }
    }

    const auto untracked_paths = find_untracked_paths(local_hash_by_path, staged_by_path, tracked_by_path);
    status.untracked_entries = compress_untracked_paths(workspace, untracked_paths);

    std::sort(status.staged_paths.begin(), status.staged_paths.end());
    std::sort(status.modified_paths.begin(), status.modified_paths.end());
    std::sort(status.modified_after_staging_paths.begin(), status.modified_after_staging_paths.end());
    std::sort(status.missing_paths.begin(), status.missing_paths.end());

    return status;
}

void print_status_info(const StatusInfo& status) {
    constexpr const char* Reset = "\033[0m";
    constexpr const char* Green = "\033[32m";
    constexpr const char* Red = "\033[31m";
    constexpr const char* Yellow = "\033[33m";

    std::cout << Grusha << "gu: ";
    std::cout << (status.master_address.empty() ? "not connected" : status.master_address) << '\n';

    std::cout << Grusha << "device_id: ";
    if (status.device_id == 0) {
        std::cout << "unknown\n";
    } else {
        std::cout << status.device_id << '\n';
    }

    const bool is_clean = status.staged_paths.empty() && status.modified_paths.empty() && status.modified_after_staging_paths.empty() && status.missing_paths.empty() && status.untracked_entries.empty();

    if (is_clean) {
        std::cout << '\n' << Grusha << "working tree clean\n";
        return;
    }

    if (!status.staged_paths.empty()) {
        std::cout << '\n' << Grusha << "Changes to be pushed:\n";
        std::cout << " (use \"pear unstage <path>...\" to unstage)\n";

        for (const auto& file : status.staged_files) {
            std::string label = "modified";

            if (file.operation == "add") {
                label = "new file";
            } else if (file.operation == "delete") {
                label = "deleted";
            }

            std::cout << " " << Green << label << ": " << file.path << Reset << '\n';
        }
    }

    if (!status.modified_after_staging_paths.empty() || !status.modified_paths.empty() || !status.missing_paths.empty()) {
        std::cout << '\n' << Grusha << "Changes not staged:\n";
        std::cout << " (use \"pear add <path>...\" to update what will be pushed)\n";

        for (const auto& path : status.modified_after_staging_paths) {
            std::cout << " " << Yellow << "modified after staging: " << path << Reset << '\n';
        }

        for (const auto& path : status.modified_paths) {
            std::cout << " " << Yellow << "modified: " << path << Reset << '\n';
        }

        for (const auto& path : status.missing_paths) {
            std::cout << " " << Yellow << "missing: " << path << Reset << '\n';
        }
    }

    if (!status.untracked_entries.empty()) {
        std::cout << '\n' << Grusha << "Untracked files:\n";
        std::cout << " (use \"pear add <path>...\" to include in what will be pushed)\n";

        for (const auto& entry : status.untracked_entries) {
            std::cout << " " << Red << entry << Reset << '\n';
        }
    }
}

} // namespace

namespace pear::cli {

void run_init(const std::filesystem::path& workspace_path) {
#ifdef PEAR_DEBUG
    std::cout << "[DEBUG] run_init called\n";
    std::cout << "[DEBUG] workspace_path: " << workspace_path << '\n';
#endif

    pear::storage::Workspace workspace = pear::storage::Workspace::init(workspace_path);
    [[maybe_unused]] pear::db::SqliteDatabase database(get_database_path(workspace));
    std::cout << Grusha << "initialized workspace at " << workspace.get_root() << '\n';
}

void run_deinit() {
#ifdef PEAR_DEBUG
    std::cout << "[DEBUG] run_deinit called\n";
#endif

    namespace fs = std::filesystem;

    pear::storage::Workspace workspace = pear::storage::Workspace::discover();

    if (pear::demon::is_alive(workspace.get_root())) {
        pear::demon::kill(workspace.get_root());

        for (int attempt = 0; attempt < 100; ++attempt) {
            if (!pear::demon::is_alive(workspace.get_root())) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        if (pear::demon::is_alive(workspace.get_root())) {
            throw std::runtime_error("failed to stop demon before deinit");
        }
    }

    for (const auto& entry : fs::directory_iterator(workspace.get_root())) {
        if (entry.is_regular_file() && entry.path().extension() == ".empty") {
            fs::remove(entry.path());
        }
    }

    fs::remove_all(workspace.get_peer_dir());
    std::cout << Grusha << "deinitialized workspace at " << workspace.get_root() << '\n';
}

void run_connect(const std::string& gu_address, const std::string& listen_address, bool is_main) {
#ifdef PEAR_DEBUG
    std::cout << "[DEBUG] run_connect called\n";
    std::cout << "[DEBUG] gu_address: " << gu_address << '\n';
    std::cout << "[DEBUG] listen_address: " << listen_address << '\n';
    std::cout << "[DEBUG] is_main: " << std::boolalpha << is_main << '\n';
#endif

    pear::storage::Workspace workspace = pear::storage::Workspace::discover();
    pear::db::SqliteDatabase database(get_database_path(workspace));

    pear::demon::spawn(workspace.get_root(), listen_address, is_main);

    try {
        if (is_main) {
            uint64_t device_id = database.registerDevice(listen_address);

            database.setMasterAddress(listen_address);
            database.setDeviceId(device_id);

            std::cout << Grusha << "connected as main node at " << listen_address << " device_id=" << device_id << '\n';
            return;
        }

        uint64_t device_id = pear::net::RemoteClient::RegisterDevice(gu_address, listen_address);
        std::vector<pear::net::WalEntryInfo> wal_entries = pear::net::RemoteClient::UpdateDB(gu_address, 0, device_id);

        database.applyWalEntries(wal_entries);
        database.setMasterAddress(gu_address);
        database.setDeviceId(device_id);

        std::cout << Grusha << "connected to " << gu_address << " as device_id=" << device_id << '\n';
    } catch (...) {
        try {
            if (pear::demon::is_alive(workspace.get_root())) {
                pear::demon::kill(workspace.get_root());
            }
        } catch (...) {
        }
        throw;
    }
}

void run_disconnect() {
#ifdef PEAR_DEBUG
    std::cout << "[DEBUG] run_disconnect called\n";
#endif

    pear::storage::Workspace workspace = pear::storage::Workspace::discover();
    pear::db::SqliteDatabase database(get_database_path(workspace));

    bool demon_was_alive = pear::demon::is_alive(workspace.get_root());

    if (demon_was_alive) {
        pear::demon::kill(workspace.get_root());
    }

    database.setMasterAddress("");
    database.setDeviceId(0);

    if (demon_was_alive) {
        std::cout << Grusha << "disconnected\n";
    } else {
        std::cout << Grusha << "already disconnected\n";
    }
}

void run_add(const std::vector<std::filesystem::path>& paths, bool all) {
#ifdef PEAR_DEBUG
    std::cout << "[DEBUG] run_add called\n";
    std::cout << "[DEBUG] all: " << std::boolalpha << all << '\n';

    if (!all) {
        for (const auto& path : paths) {
            std::cout << "[DEBUG] path: " << path << '\n';
        }
    }
#endif

    namespace fs = std::filesystem;

    pear::storage::Workspace workspace = pear::storage::Workspace::discover();
    pear::db::SqliteDatabase database(get_database_path(workspace));

    bool had_errors = false;

    auto stage_file = [&](const fs::path& file_path) {
        try {
            if (file_path.extension() == ".empty") {
                return; 
            }

            const std::string relative_path = workspace.get_relative_path(file_path).generic_string();
            const std::string object_hash = pear::storage::get_file_hash(file_path);
            const auto current_object_hash = database.getObjectHashByPath(relative_path);

            if (current_object_hash && *current_object_hash == object_hash) {
                database.unstageFile(relative_path);
                std::cout << Grusha << "already up to date " << relative_path << '\n';
                return;
            }

            if (!workspace.has_objectfile(object_hash)) {
                workspace.create_objectfile(object_hash, file_path);
            }

            const std::string operation = current_object_hash ? "update" : "add";
            database.stageFile(relative_path, object_hash, file_path.string(), operation);

            std::cout << Grusha << "staged " << relative_path << '\n';
        } catch (const std::exception& error) {
            std::cerr << "error: failed to stage " << file_path << ": " << error.what() << '\n';
            had_errors = true;
        }
    };

    auto stage_path = [&](const fs::path& path) {
        try {
            const auto files = workspace.collect_files(path);

            for (const auto& file_path : files) {
                stage_file(file_path);
            }
        } catch (const std::exception& error) {
            std::cerr << "error: failed to stage " << path << ": " << error.what() << '\n';
            had_errors = true;
        }
    };

    if (all) {
        stage_path(workspace.get_root());
    } else {
        for (const auto& path : paths) {
            stage_path(path);
        }
    }

    if (had_errors) {
        throw std::runtime_error("failed to stage some files");
    }
}

void run_unstage(const std::vector<std::filesystem::path>& paths, bool all) {
#ifdef PEAR_DEBUG
    std::cout << "[DEBUG] run_unstage called\n";
    std::cout << "[DEBUG] all: " << std::boolalpha << all << '\n';

    if (!all) {
        for (const auto& path : paths) {
            std::cout << "[DEBUG] path: " << path << '\n';
        }
    }
#endif

    pear::storage::Workspace workspace = pear::storage::Workspace::discover();
    pear::db::SqliteDatabase database(get_database_path(workspace));

    if (all) {
        database.clearStaging();
        std::cout << Grusha << "cleared staging\n";
        return;
    }

    const auto staged_files = database.getStagedFiles();

    if (staged_files.empty()) {
        std::cout << Grusha << "nothing to unstage\n";
        return;
    }

    bool had_errors = false;
    std::unordered_set<std::string> removed_paths;

    auto should_unstage = [](const std::string& staged_path, const std::string& target_path) {
        if (target_path == ".") {
            return true;
        }

        if (staged_path == target_path) {
            return true;
        }

        const std::string prefix = target_path + "/";
        return staged_path.rfind(prefix, 0) == 0;
    };

    for (const auto& path : paths) {
        try {
            const std::string target_path = workspace.get_relative_path(path).generic_string();

            std::vector<std::string> paths_to_unstage;

            for (const auto& file : staged_files) {
                if (removed_paths.contains(file.path)) {
                    continue;
                }

                if (should_unstage(file.path, target_path)) {
                    paths_to_unstage.push_back(file.path);
                }
            }

            if (paths_to_unstage.empty()) {
                std::cerr << "error: nothing staged under " << target_path << '\n';
                had_errors = true;
                continue;
            }

            std::sort(paths_to_unstage.begin(), paths_to_unstage.end());

            for (const auto& staged_path : paths_to_unstage) {
                database.unstageFile(staged_path);
                removed_paths.insert(staged_path);
                std::cout << Grusha << "unstaged " << staged_path << '\n';
            }
        } catch (const std::exception& error) {
            std::cerr << "error: failed to unstage " << path << ": " << error.what() << '\n';
            had_errors = true;
        }
    }

    if (had_errors) {
        throw std::runtime_error("failed to unstage some files");
    }
}

void run_update() {
#ifdef PEAR_DEBUG
    std::cout << "[DEBUG] run_update called\n";
#endif

    sync_with_master(true);
}

void run_ls() {
#ifdef PEAR_DEBUG
    std::cout << "[DEBUG] run_ls called\n";
#endif

    struct TreeNode {
        std::map<std::string, TreeNode> dirs;
        std::map<std::string, pear::net::FileUpdateInfo> files;
    };

    auto split_path = [](const std::string& path) {
        std::vector<std::string> parts;
        std::string current;

        for (char character : path) {
            if (character == '/') {
                if (!current.empty()) {
                    parts.push_back(current);
                    current.clear();
                }
            } else {
                current.push_back(character);
            }
        }

        if (!current.empty()) {
            parts.push_back(current);
        }

        return parts;
    };

    auto short_hash = [](const std::string& object_hash) {
        if (object_hash.size() <= 12) {
            return object_hash;
        }

        return object_hash.substr(0, 12);
    };

    pear::storage::Workspace workspace = pear::storage::Workspace::discover();
    pear::db::SqliteDatabase database(get_database_path(workspace));

    const auto files = database.getAllFiles();

    if (files.empty()) {
        std::cout << Grusha << "workspace is empty\n";
        return;
    }

    TreeNode root;

    for (const auto& file : files) {
        const auto parts = split_path(file.path);

        if (parts.empty()) {
            continue;
        }

        TreeNode* current = &root;

        for (size_t part_index = 0; part_index + 1 < parts.size(); ++part_index) {
            current = &current->dirs[parts[part_index]];
        }

        current->files[parts.back()] = file;
    }

    std::cout << Grusha << ".\n";

    auto print_tree = [&](auto&& self, const TreeNode& node, const std::string& prefix) -> void {
        std::vector<std::pair<std::string, const TreeNode*>> dirs;
        std::vector<std::pair<std::string, const pear::net::FileUpdateInfo*>> files_to_print;

        for (const auto& [name, child] : node.dirs) {
            dirs.push_back({name, &child});
        }

        for (const auto& [name, file] : node.files) {
            files_to_print.push_back({name, &file});
        }

        const size_t total_count = dirs.size() + files_to_print.size();
        size_t printed_count = 0;

        for (const auto& [name, child] : dirs) {
            ++printed_count;

            const bool is_last = printed_count == total_count;
            std::cout << prefix << (is_last ? "└── " : "├── ") << name << "/\n";

            self(self, *child, prefix + (is_last ? "    " : "│   "));
        }

        for (const auto& [name, file] : files_to_print) {
            ++printed_count;

            const bool is_last = printed_count == total_count;
            const std::string owner_address = database.getDeviceAddress(file->owner_device_id);

            std::cout << prefix << (is_last ? "└── " : "├── ") << name << "  [v" << file->version << ", owner:" << file->owner_device_id;

            if (!owner_address.empty()) {
                std::cout << " " << owner_address;
            }

            std::cout << ", obj:" << short_hash(file->object_hash) << "]\n";
        }
    };

    print_tree(print_tree, root, "");
}

void run_push() {
#ifdef PEAR_DEBUG
    std::cout << "[DEBUG] run_push called\n";
#endif

    pear::storage::Workspace workspace = pear::storage::Workspace::discover();

    {
        pear::db::SqliteDatabase database(get_database_path(workspace));
        const std::string master_address = database.getMasterAddress();
        const uint64_t device_id = database.getDeviceId();

        if (master_address.empty()) {
            throw std::runtime_error("not connected: master address is empty");
        }
        if (device_id == 0) {
            throw std::runtime_error("not connected: device id is unknown");
        }
    }

    sync_with_master(false);

    pear::db::SqliteDatabase database(get_database_path(workspace));
    const std::string master_address = database.getMasterAddress();
    const uint64_t device_id = database.getDeviceId();
    const auto staged_files = database.getStagedFiles();

    if (staged_files.empty()) {
        std::cout << Grusha << "nothing to push\n";
        return;
    }

    bool had_errors = false;
    std::vector<pear::net::WalEntryInfo> wal_entries;
    std::vector<std::string> pushed_paths;

    for (const auto& file : staged_files) {
        try {
            if (file.operation != "add" && file.operation != "update") {
                throw std::runtime_error("unsupported staged operation: " + file.operation);
            }

            if (!workspace.has_objectfile(file.object_hash)) {
                throw std::runtime_error("staged object does not exist: " + file.object_hash);
            }

            const uint64_t version = database.getNextVersion(file.path);

            pear::net::WalEntryInfo entry {};
            entry.op_type = pear::net::WalOpTypeInfo::kFileUpdate;
            entry.file.path = file.path;
            entry.file.object_hash = file.object_hash;
            entry.file.version = version;
            entry.file.owner_device_id = device_id;

            wal_entries.push_back(entry);
            pushed_paths.push_back(file.path);
        } catch (const std::exception& error) {
            std::cerr << "error: failed to prepare push for " << file.path << ": " << error.what() << '\n';
            had_errors = true;
        }
    }

    if (wal_entries.empty()) {
        throw std::runtime_error("failed to prepare push");
    }

    std::vector<uint64_t> assigned_seq_ids;
    if (!pear::net::RemoteClient::PushWAL(master_address, device_id, wal_entries, assigned_seq_ids)) {
        throw std::runtime_error("failed to push wal to main node");
    }

    for (const auto& path : pushed_paths) {
        database.unstageFile(path);
    }

    sync_with_master(false);

    for (const auto& path : pushed_paths) {
        std::cout << Grusha << "pushed " << path << '\n';
    }

    if (had_errors) {
        throw std::runtime_error("failed to push some staged files");
    }
}

void run_pull(const std::vector<std::string>& targets) {
#ifdef PEAR_DEBUG
    std::cout << "[DEBUG] run_pull called\n";

    for (const auto& target : targets) {
        std::cout << "[DEBUG] target: " << target << '\n';
    }
#endif

    namespace fs = std::filesystem;

    pear::storage::Workspace workspace = pear::storage::Workspace::discover();

    {
        pear::db::SqliteDatabase database(get_database_path(workspace));

        const std::string master_address = database.getMasterAddress();
        const uint64_t device_id = database.getDeviceId();

        if (master_address.empty()) {
            throw std::runtime_error("not connected: master address is empty");
        }

        if (device_id == 0) {
            throw std::runtime_error("not connected: device id is unknown");
        }
    }

    sync_with_master(false);

    pear::db::SqliteDatabase database(get_database_path(workspace));

    const uint64_t device_id = database.getDeviceId();
    const auto all_files = database.getAllFiles();

    bool had_errors = false;

    auto remove_empty_suffix = [](std::string path) {
        const std::string empty_suffix = ".empty";

        if (path.size() >= empty_suffix.size() &&
            path.compare(path.size() - empty_suffix.size(), empty_suffix.size(), empty_suffix) == 0) {
            path.erase(path.size() - empty_suffix.size());
        }

        return path;
    };

    auto pull_file = [&](const pear::net::FileUpdateInfo& file) {
        if (file.object_hash.empty()) {
            throw std::runtime_error("file has empty object hash");
        }

        const fs::path destination_path = workspace.get_root() / file.path;
        fs::create_directories(destination_path.parent_path());

        if (workspace.has_objectfile(file.object_hash)) {
            fs::copy_file(workspace.get_objectfile_path(file.object_hash), destination_path, fs::copy_options::overwrite_existing);
        } else {
            const std::string owner_address = database.getDeviceAddress(file.owner_device_id);

            if (owner_address.empty()) {
                throw std::runtime_error("owner address is unknown");
            }

            pear::net::RemoteClient::DownloadFile(owner_address, file.object_hash, device_id, destination_path.string());
        }

        const fs::path empty_path = workspace.get_root() / (file.path + ".empty");
        if (fs::exists(empty_path)) {
            fs::remove(empty_path);
        }

        std::cout << Grusha << "pulled " << file.path << '\n';
    };

    for (const auto& target : targets) {
        try {
            const std::string cleaned_target = remove_empty_suffix(target);
            const std::string target_path = workspace.get_relative_path(fs::path(cleaned_target)).generic_string();

            std::vector<pear::net::FileUpdateInfo> files_to_pull;

            auto file_info = database.getFileInfoByPath(target_path, 0);
            if (file_info) {
                files_to_pull.push_back(*file_info);
            } else {
                std::string prefix = target_path;

                while (!prefix.empty() && prefix.back() == '/') {
                    prefix.pop_back();
                }

                prefix += '/';

                for (const auto& file : all_files) {
                    if (file.path.rfind(prefix, 0) == 0) {
                        files_to_pull.push_back(file);
                    }
                }
            }

            if (files_to_pull.empty()) {
                throw std::runtime_error("file or directory not found");
            }

            std::sort(
                files_to_pull.begin(),
                files_to_pull.end(),
                [](const auto& lhs, const auto& rhs) {
                    return lhs.path < rhs.path;
                }
            );

            for (const auto& file : files_to_pull) {
                pull_file(file);
            }
        } catch (const std::exception& error) {
            std::cerr << "error: failed to pull " << target << ": " << error.what() << '\n';
            had_errors = true;
        }
    }

    if (had_errors) {
        throw std::runtime_error("failed to pull some files");
    }
}

void run_status() {
#ifdef PEAR_DEBUG
    std::cout << "[DEBUG] run_status called\n";
#endif

    pear::storage::Workspace workspace = pear::storage::Workspace::discover();
    pear::db::SqliteDatabase database(get_database_path(workspace));

    const StatusInfo status = collect_status_info(workspace, database);
    print_status_info(status);
}

} // namespace pear::cli