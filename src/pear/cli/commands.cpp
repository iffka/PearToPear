#include <pear/cli/commands.hpp>

#include <pear/db/sqlite_database.hpp>
#include <pear/fs/workspace.hpp>
#include <pear/demon/demon.hpp>
#include <pear/net/remote_client.hpp>
#include <pear/net/types.hpp>

#include "command_helpers.hpp"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <unordered_set>
#include <algorithm>
#include <chrono>
#include <thread>

namespace {

constexpr const char* Grusha = "🍐 ";

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
    const fs::path workspace_root = fs::weakly_canonical(workspace.get_root());
    const fs::path peer_dir = fs::weakly_canonical(workspace.get_peer_dir());

    bool had_errors = false;

    auto try_stage_file = [&](const fs::path& input_path, bool is_resolved_path) {
        try {
            fs::path resolved_path = is_resolved_path ? input_path : resolve_existing_file(input_path);

            if (!is_path_within(workspace_root, resolved_path)) {
                throw std::runtime_error("path is outside workspace");
            }
            if (is_path_within(peer_dir, resolved_path)) {
                throw std::runtime_error("path is inside .peer");
            }

            const std::string file_name = resolved_path.filename().string();

            if (file_name.empty()) {
                throw std::runtime_error("path has empty file name");
            }
            if (resolved_path.extension() == ".empty") {
                throw std::runtime_error("cannot stage .empty placeholder");
            }

            database.stageFile(file_name, file_name, resolved_path.string());
            std::cout << Grusha << "staged " << file_name << '\n';
        } catch (const std::exception& error) {
            std::cerr << "error: failed to stage " << input_path << ": " << error.what() << '\n';
            had_errors = true;
        }
    };

    if (all) {
        for (const auto& entry : fs::directory_iterator(workspace_root)) {
            if (!entry.is_regular_file()) {
                continue;
            }

            fs::path resolved_path = fs::weakly_canonical(entry.path());

            if (resolved_path.extension() == ".empty") {
                continue;
            }

            try_stage_file(resolved_path, true);
        }
    } else {
        for (const auto& path : paths) {
            try_stage_file(path, false);
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
    std::unordered_set<std::string> staged_file_ids;
    for (const auto& file : staged_files) {
        staged_file_ids.insert(file.file_id);
    }

    bool had_errors = false;

    for (const auto& path : paths) {
        std::string file_id = path.filename().string();

        if (file_id.empty()) {
            std::cerr << "error: failed to unstage: invalid path: " << path << '\n';
            had_errors = true;
            continue;
        }

        if (!staged_file_ids.contains(file_id)) {
            std::cerr << "error: failed to unstage " << file_id << ": file is not staged\n";
            had_errors = true;
            continue;
        }

        database.unstageFile(file_id);
        std::cout << Grusha << "unstaged " << file_id << '\n';
    }

    if (had_errors) {
        throw std::runtime_error("failed to unstage some files");
    }
}

void run_update() {
#ifdef PEAR_DEBUG
    std::cout << "[DEBUG] run_update called\n";
#endif

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
    std::unordered_set<std::string> desired_empty_names;

    for (const auto& file : tracked_files) {
        if (!fs::exists(workspace.get_root() / file.name)) {
            desired_empty_names.insert(file.name);
        }
    }

    for (const auto& entry : fs::directory_iterator(workspace.get_root())) {
        if (!entry.is_regular_file() || entry.path().extension() != ".empty") {
            continue;
        }

        const std::string file_name = entry.path().stem().string();

        if (!desired_empty_names.contains(file_name)) {
            fs::remove(entry.path());
        }
    }

    std::vector<std::string> empty_names(desired_empty_names.begin(), desired_empty_names.end());
    std::sort(empty_names.begin(), empty_names.end());
    workspace.create_all_empty_files(empty_names);

    if (wal_entries.empty()) {
        std::cout << Grusha << "already up to date\n";
    } else {
        std::cout << Grusha << "applied " << wal_entries.size() << " wal entries\n";
    }
}

void run_ls() {
#ifdef PEAR_DEBUG
    std::cout << "[DEBUG] run_ls called\n";
#endif

    pear::storage::Workspace workspace = pear::storage::Workspace::discover();
    pear::db::SqliteDatabase database(get_database_path(workspace));
    const auto files = database.getAllFiles();
    if (files.empty()) {
        std::cout << Grusha << "workspace is empty\n";
        return;
    }
    for (const auto& file : files) {
        std::cout << Grusha << file.name << " version:" << file.version << " owner:" << file.owner_device_id << '\n';
    }
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

    run_update();

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
    std::vector<std::string> pushed_file_ids;
    std::vector<std::string> pushed_file_names;

    for (const auto& file : staged_files) {
        try {
            const std::filesystem::path local_path = resolve_existing_file(file.local_path);
            const uint64_t version = database.getNextVersion(file.file_id);
            const std::string object_name = file.file_id + "." + std::to_string(version);

            workspace.create_objectfile(object_name, local_path);

            pear::net::WalEntryInfo entry {};
            entry.op_type = pear::net::WalOpTypeInfo::kFileUpdate;
            entry.file.file_id = file.file_id;
            entry.file.name = file.name;
            entry.file.version = version;
            entry.file.owner_device_id = device_id;

            wal_entries.push_back(entry);
            pushed_file_ids.push_back(file.file_id);
            pushed_file_names.push_back(file.name);
        } catch (const std::exception& error) {
            std::cerr << "error: failed to prepare push for " << file.name << ": " << error.what() << '\n';
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

    for (const auto& file_id : pushed_file_ids) {
        database.unstageFile(file_id);
    }

    run_update();

    for (const auto& file_name : pushed_file_names) {
        std::cout << Grusha << "pushed " << file_name << '\n';
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

    run_update();

    pear::db::SqliteDatabase database(get_database_path(workspace));
    const uint64_t device_id = database.getDeviceId();

    bool had_errors = false;

    for (const auto& target : targets) {
        try {
            auto file_info = database.getFileInfo(target, 0);

            if (!file_info) {
                for (const auto& file : database.getAllFiles()) {
                    if (file.name == target) {
                        file_info = file;
                        break;
                    }
                }
            }

            if (!file_info) {
                throw std::runtime_error("file not found");
            }

            const std::string owner_address = database.getDeviceAddress(file_info->owner_device_id);

            if (owner_address.empty()) {
                throw std::runtime_error("owner address is unknown");
            }

            const fs::path destination_path = workspace.get_root() / file_info->name;

            pear::net::RemoteClient::DownloadFile(
                owner_address,
                file_info->file_id,
                file_info->version,
                device_id,
                destination_path.string()
            );

            const fs::path empty_path = workspace.get_root() / (file_info->name + ".empty");
            if (fs::exists(empty_path)) {
                fs::remove(empty_path);
            }

            std::cout << Grusha << "pulled " << file_info->name << '\n';
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

    namespace fs = std::filesystem;

    constexpr const char* Reset = "\033[0m";
    constexpr const char* Green = "\033[32m";
    constexpr const char* Red = "\033[31m";

    pear::storage::Workspace workspace = pear::storage::Workspace::discover();
    pear::db::SqliteDatabase database(get_database_path(workspace));
    const auto master_address = database.getMasterAddress();
    const auto device_id = database.getDeviceId();
    const auto staged_files = database.getStagedFiles();
    const auto tracked_files = database.getAllFiles();
    const fs::path workspace_root = fs::weakly_canonical(workspace.get_root());
    const fs::path peer_dir = fs::weakly_canonical(workspace.get_peer_dir());

    std::unordered_set<std::string> staged_file_ids;
    for (const auto& file : staged_files) {
        staged_file_ids.insert(file.file_id);
    }

    std::unordered_set<std::string> tracked_file_ids;
    for (const auto& file : tracked_files) {
        tracked_file_ids.insert(file.file_id);
    }

    std::vector<std::string> untracked_entries;
    for (const auto& entry : fs::directory_iterator(workspace_root)) {
        const fs::path entry_path = fs::weakly_canonical(entry.path());
        if (entry_path == peer_dir) {
            continue;
        }
        const std::string name = entry_path.filename().string();
        if (name.empty() || entry_path.extension() == ".empty") {
            continue;
        }
        if (entry.is_directory()) {
            untracked_entries.push_back(name + "/");
            continue;
        }
        if (!entry.is_regular_file()) {
            continue;
        }
        if (staged_file_ids.contains(name) || tracked_file_ids.contains(name)) {
            continue;
        }
        untracked_entries.push_back(name);
    }

    std::sort(untracked_entries.begin(), untracked_entries.end());

    std::cout << Grusha << "gu: ";
    if (master_address.empty()) {
        std::cout << "not connected\n";
    } else {
        std::cout << master_address << '\n';
    }

    std::cout << Grusha << "device_id: ";
    if (device_id == 0) {
        std::cout << "unknown\n";
    } else {
        std::cout << device_id << '\n';
    }

    if (staged_files.empty() && untracked_entries.empty()) {
        std::cout << '\n' << Grusha << "working tree clean\n";
        return;
    }

    if (!staged_files.empty()) {
        std::cout << '\n' << Grusha << "Changes to be pushed:\n";
        std::cout << "  (use \"pear unstage <file>...\" to unstage)\n";
        std::cout << "  (if you changed a staged file, run \"pear add <file>\" again before push)\n";
        for (const auto& file : staged_files) {
            std::cout << "        staged:   " << Green << file.name << Reset << '\n';
        }
    }

    if (!untracked_entries.empty()) {
        std::cout << '\n' << Grusha << "Untracked files:\n";
        std::cout << "  (use \"pear add <file>...\" to include in what will be pushed)\n";
        for (const auto& entry : untracked_entries) {
            std::cout << "        " << Red << entry << Reset << '\n';
        }
    }
}

} // namespace pear::cli