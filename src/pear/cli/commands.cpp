#include <pear/cli/commands.hpp>

#include <iostream>
#include <fstream>
#include <filesystem>
#include <stdexcept>
#include <algorithm>
#include <ctime>

#include <pear/fs/workspace.hpp>
#include <pear/net/remote_client.hpp>
#include <pear/net/db_contract.hpp>
#include <pear/net/fs_contract.hpp>

namespace pear {
    extern std::filesystem::path g_workspace_root;
    extern std::shared_ptr<pear::net::DatabaseFacade> g_db;
    extern std::shared_ptr<pear::net::FilesystemFacade> g_fs;
    extern std::unique_ptr<pear::net::Node> g_node;
    extern bool g_node_running;
    extern std::string g_listen_address; // добавлено для доступа
}

namespace {
    std::filesystem::path staging_path() {
        return pear::g_workspace_root / ".peer" / "staging.txt";
    }

    std::vector<std::string> read_staging() {
        std::vector<std::string> files;
        std::ifstream f(staging_path());
        std::string line;
        while (std::getline(f, line)) {
            if (!line.empty()) files.push_back(line);
        }
        return files;
    }

    void write_staging(const std::vector<std::string>& files) {
        std::ofstream f(staging_path());
        for (const auto& name : files) f << name << "\n";
    }

    void add_to_staging(const std::vector<std::filesystem::path>& paths) {
        auto current = read_staging();
        for (const auto& p : paths) {
            std::string name = p.filename().string();
            if (std::find(current.begin(), current.end(), name) == current.end())
                current.push_back(name);
        }
        write_staging(current);
    }

    void remove_from_staging(const std::vector<std::filesystem::path>& paths) {
        auto current = read_staging();
        std::vector<std::string> new_list;
        for (const auto& name : current) {
            bool keep = true;
            for (const auto& p : paths) {
                if (p.filename().string() == name) { keep = false; break; }
            }
            if (keep) new_list.push_back(name);
        }
        write_staging(new_list);
    }

    void add_all_files_in_directory() {
        std::vector<std::string> all_files;
        for (const auto& entry : std::filesystem::directory_iterator(pear::g_workspace_root)) {
            if (entry.is_regular_file() && entry.path().filename() != ".peer") {
                all_files.push_back(entry.path().filename().string());
            }
        }
        auto current = read_staging();
        for (const auto& f : all_files) {
            if (std::find(current.begin(), current.end(), f) == current.end())
                current.push_back(f);
        }
        write_staging(current);
    }
} // anonymous namespace

namespace pear::cli {

void run_init(const std::filesystem::path&) {
    // handled in main
}

void run_deinit() {
    if (g_node_running) {
        g_node->stop();
        g_node_running = false;
    }
    std::filesystem::remove_all(g_workspace_root / ".peer");
    std::cout << "Workspace removed.\n";
}

void run_connect(const std::string& repo_id) {
    std::string my_address = g_listen_address; // берём из глобальной переменной (загружена из конфига)
    if (my_address.empty()) {
        throw std::runtime_error("Listen address not set. Run 'pear init' first.");
    }
    uint64_t device_id = 0;
    try {
        device_id = pear::net::RemoteClient::RegisterDevice(repo_id, my_address);
    } catch (const std::exception& e) {
        throw std::runtime_error("RegisterDevice failed: " + std::string(e.what()));
    }
    g_db->setMasterAddress(repo_id);
    g_db->setDeviceId(device_id);
    std::cout << "Connected. Device ID: " << device_id << std::endl;
}

void run_disconnect() {
    g_db->setMasterAddress("");
    g_db->setDeviceId(0);
    std::cout << "Disconnected.\n";
}

void run_add(const std::vector<std::filesystem::path>& paths, bool all) {
    if (all) {
        add_all_files_in_directory();
        std::cout << "Staged all files in workspace.\n";
    } else {
        add_to_staging(paths);
        std::cout << "Staged " << paths.size() << " file(s).\n";
    }
}

void run_unstage(const std::vector<std::filesystem::path>& paths, bool all) {
    if (all) {
        write_staging({});
        std::cout << "Unstaged all files.\n";
    } else {
        remove_from_staging(paths);
        std::cout << "Unstaged " << paths.size() << " file(s).\n";
    }
}

void run_update() {
    std::string gu_addr = g_db->getMasterAddress();
    if (gu_addr.empty()) throw std::runtime_error("Not connected to any master. Run 'pear connect'.");
    uint64_t device_id = g_db->getDeviceId();
    uint64_t last_seq = g_db->getLastSeqId();
    std::vector<pear::net::WalEntryInfo> entries;
    try {
        entries = pear::net::RemoteClient::UpdateDB(gu_addr, last_seq, device_id);
    } catch (const std::exception& e) {
        throw std::runtime_error("UpdateDB failed: " + std::string(e.what()));
    }
    g_db->applyWalEntries(entries);
    std::cout << "Applied " << entries.size() << " new WAL entries.\n";
}

void run_ls() {
    auto files = g_db->getAllFiles();
    for (const auto& f : files) {
        std::cout << f.name << " (version " << f.version << ", owner " << f.owner_device_id << ")\n";
    }
}

void run_push() {
    std::string gu_addr = g_db->getMasterAddress();
    if (gu_addr.empty()) throw std::runtime_error("Not connected to any master. Run 'pear connect'.");
    uint64_t device_id = g_db->getDeviceId();
    auto staged = read_staging();
    if (staged.empty()) {
        std::cout << "Nothing to push.\n";
        return;
    }

    std::vector<pear::net::WalEntryInfo> entries;
    std::vector<std::filesystem::path> source_paths;
    for (const auto& filename : staged) {
        auto opt = g_db->getFileInfo(filename, 0);
        uint64_t new_version = opt.has_value() ? opt->version + 1 : 1;
        pear::net::WalEntryInfo entry;
        entry.seq_id = 0;
        entry.timestamp = std::time(nullptr);
        entry.op_type = 0; // FILE_UPDATE
        entry.file.file_id = filename;
        entry.file.name = filename;
        entry.file.version = new_version;
        entry.file.owner_device_id = device_id;
        entries.push_back(entry);
        source_paths.push_back(g_workspace_root / filename);
    }

    std::vector<uint64_t> assigned_ids;
    bool ok = pear::net::RemoteClient::PushWAL(gu_addr, device_id, entries, assigned_ids);
    if (!ok) throw std::runtime_error("PushWAL failed");

    for (size_t i = 0; i < entries.size(); ++i) {
        entries[i].seq_id = assigned_ids[i];
        g_db->addWalEntry(entries[i]);
        if (std::filesystem::exists(source_paths[i])) {
            g_fs->storeObject(entries[i].file.file_id, entries[i].file.version, source_paths[i]);
        }
    }
    write_staging({});
    std::cout << "Pushed " << entries.size() << " file(s).\n";
}

void run_pull(const std::vector<std::string>& targets) {
    for (const auto& target : targets) {
        auto opt = g_db->getFileInfo(target, 0);
        if (!opt) {
            std::cerr << "File not found in DB: " << target << std::endl;
            continue;
        }
        auto& info = opt.value();
        std::string vu_address = g_db->getDeviceAddress(info.owner_device_id);
        if (vu_address.empty()) {
            std::cerr << "No address for owner of " << target << std::endl;
            continue;
        }
        std::string dest = (g_workspace_root / info.name).string();
        try {
            pear::net::RemoteClient::DownloadFile(vu_address, info.file_id, info.version, g_db->getDeviceId(), dest);
            std::cout << "Downloaded " << info.name << " from " << vu_address << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Download failed for " << target << ": " << e.what() << std::endl;
        }
    }
}

void run_status() {
    auto staged = read_staging();
    if (staged.empty()) {
        std::cout << "No staged files.\n";
    } else {
        std::cout << "Staged files:\n";
        for (const auto& f : staged) std::cout << "  " << f << std::endl;
    }
}

} // namespace pear::cli