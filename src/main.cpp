#include <filesystem>
#include <iostream>
#include <memory>
#include <thread>
#include <csignal>
#include <map>
#include <fstream>

#include <CLI/CLI.hpp>

#include <pear/cli/commands.hpp>
#include <pear/fs/workspace.hpp>
#include <pear/net/node.hpp>
#include <pear/net/db_contract.hpp>
#include <pear/net/fs_contract.hpp>

namespace pear {
    std::filesystem::path g_workspace_root;
    std::shared_ptr<pear::net::DatabaseFacade> g_db;
    std::shared_ptr<pear::net::FilesystemFacade> g_fs;
    std::unique_ptr<pear::net::Node> g_node;
    bool g_node_running = false;
    std::string g_listen_address; // собственный адрес для прослушивания
}

// ========== Заглушка DatabaseFacade с хранением конфигурации в памяти ==========
class FakeDatabaseFacade : public pear::net::DatabaseFacade {
private:
    std::map<std::string, std::string> config_;
    uint64_t last_seq_id_ = 0;
public:
    std::vector<pear::net::WalEntryInfo> getWalEntriesSince(uint64_t) override { return {}; }
    void applyWalEntries(const std::vector<pear::net::WalEntryInfo>&) override {}
    std::optional<pear::net::FileUpdateInfo> getFileInfo(const std::string&, uint64_t) override {
        return pear::net::FileUpdateInfo{"test_id", "test.txt", 1, 1};
    }
    uint64_t addWalEntry(const pear::net::WalEntryInfo&) override { return ++last_seq_id_; }
    uint64_t getLastSeqId() override { return last_seq_id_; }
    std::vector<pear::net::FileUpdateInfo> getAllFiles() override {
        return {{"file1", "file1.txt", 1, 1}, {"file2", "file2.cpp", 2, 1}};
    }
    void clearStaging() override {}
    uint64_t registerDevice(const std::string&) override { return 1; }
    std::string getDeviceAddress(uint64_t) override { return "localhost:50051"; }

    void setMasterAddress(const std::string& addr) override { config_["master_address"] = addr; }
    std::string getMasterAddress() override {
        auto it = config_.find("master_address");
        return (it != config_.end()) ? it->second : "";
    }
    void setDeviceId(uint64_t id) override { config_["device_id"] = std::to_string(id); }
    uint64_t getDeviceId() override {
        auto it = config_.find("device_id");
        return (it != config_.end()) ? std::stoull(it->second) : 0;
    }
};

class FakeFilesystemFacade : public pear::net::FilesystemFacade {
public:
    std::filesystem::path getObjectPath(const std::string& file_id, uint64_t) override {
        return std::filesystem::path(".peer/obj") / file_id;
    }
    void incrementDownloadCounter(const std::string&, uint64_t) override {}
    void decrementDownloadCounter(const std::string&, uint64_t) override {}
    void tryDeleteOldVersion(const std::string&, uint64_t) override {}
    std::filesystem::path storeObject(const std::string& file_id, uint64_t, const std::filesystem::path& source) override {
        auto dest = std::filesystem::path(".peer/obj") / file_id;
        std::filesystem::create_directories(dest.parent_path());
        std::filesystem::copy_file(source, dest, std::filesystem::copy_options::overwrite_existing);
        return dest;
    }
};

void signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        if (pear::g_node && pear::g_node_running) {
            pear::g_node->stop();
        }
        std::exit(0);
    }
}

void start_node_if_needed() {
    if (!pear::g_node_running && !pear::g_workspace_root.empty() && !pear::g_listen_address.empty()) {
        bool is_master = (pear::g_db->getMasterAddress().empty()); // если нет ГУ, считаем себя ГУ
        pear::g_node = std::make_unique<pear::net::Node>(pear::g_db, pear::g_fs, is_master);
        pear::g_node->start(pear::g_listen_address, false);
        pear::g_node_running = true;
    }
}

// Вспомогательные функции для работы с конфигом (файл .peer/config)
void save_listen_address(const std::filesystem::path& workspace_root, const std::string& addr) {
    auto config_path = workspace_root / ".peer" / "config";
    std::ofstream f(config_path);
    f << "listen_address=" << addr << "\n";
}

std::string load_listen_address(const std::filesystem::path& workspace_root) {
    auto config_path = workspace_root / ".peer" / "config";
    if (!std::filesystem::exists(config_path)) return "";
    std::ifstream f(config_path);
    std::string line;
    while (std::getline(f, line)) {
        if (line.rfind("listen_address=", 0) == 0) {
            return line.substr(15);
        }
    }
    return "";
}

int main(int argc, char** argv) {
    CLI::App app{"Pear - peer-to-peer storage"};

    auto init_cmd = app.add_subcommand("init", "Initialize a new Pear workspace");
    std::string listen_addr;
    init_cmd->add_option("--listen", listen_addr, "Address and port to listen on (e.g., 0.0.0.0:50051)")->default_val("127.0.0.1:50051");

    auto connect_cmd = app.add_subcommand("connect", "Connect to a Pear network (set master address)");
    auto update_cmd = app.add_subcommand("update", "Synchronize metadata with network");
    auto push_cmd = app.add_subcommand("push", "Push staged files to network");
    auto pull_cmd = app.add_subcommand("pull", "Download files from network");
    auto add_cmd = app.add_subcommand("add", "Add files to staging");
    auto unstage_cmd = app.add_subcommand("unstage", "Remove files from staging");
    auto status_cmd = app.add_subcommand("status", "Show staging status");
    auto ls_cmd = app.add_subcommand("ls", "List files known in network");
    auto deinit_cmd = app.add_subcommand("deinit", "Remove local workspace (stop node)");
    auto disconnect_cmd = app.add_subcommand("disconnect", "Disconnect from network (stop node)");

    std::string repo_id;
    connect_cmd->add_option("repo_id", repo_id, "Address of master node (ip:port)")->required();

    std::vector<std::string> targets;
    pull_cmd->add_option("targets", targets, "File names to download")->required();

    std::vector<std::filesystem::path> add_paths;
    bool add_all = false;
    add_cmd->add_option("paths", add_paths, "Paths to stage");
    add_cmd->add_flag("--all", add_all, "Stage all changed files");

    std::vector<std::filesystem::path> unstage_paths;
    bool unstage_all = false;
    unstage_cmd->add_option("paths", unstage_paths, "Paths to unstage");
    unstage_cmd->add_flag("--all", unstage_all, "Unstage all");

    init_cmd->callback([&listen_addr]() {
        auto ws = pear::storage::Workspace::init(".");
        pear::g_workspace_root = ws.get_root();
        pear::g_db = std::make_shared<FakeDatabaseFacade>();
        pear::g_fs = std::make_shared<FakeFilesystemFacade>();
        pear::g_listen_address = listen_addr;
        // Сохраняем адрес в конфиг
        save_listen_address(pear::g_workspace_root, listen_addr);
        start_node_if_needed();
        std::cout << "Workspace initialized at " << pear::g_workspace_root << std::endl;
        std::cout << "Listening on " << listen_addr << std::endl;
    });

    connect_cmd->callback([&]() {
        if (pear::g_workspace_root.empty()) {
            throw std::runtime_error("No workspace. Run 'pear init' first.");
        }
        // Убедимся, что адрес прослушивания загружен из конфига
        if (pear::g_listen_address.empty()) {
            auto loaded = load_listen_address(pear::g_workspace_root);
            if (!loaded.empty()) {
                pear::g_listen_address = loaded;
            } else {
                throw std::runtime_error("Listen address not found. Re-run 'pear init' with --listen.");
            }
        }
        pear::cli::run_connect(repo_id);
    });

    update_cmd->callback([]() { pear::cli::run_update(); });
    push_cmd->callback([]() { pear::cli::run_push(); });
    pull_cmd->callback([&]() { pear::cli::run_pull(targets); });
    add_cmd->callback([&]() {
        if (!add_all && add_paths.empty()) throw CLI::ValidationError("add", "Specify paths or use --all");
        pear::cli::run_add(add_paths, add_all);
    });
    unstage_cmd->callback([&]() {
        if (!unstage_all && unstage_paths.empty()) throw CLI::ValidationError("unstage", "Specify paths or use --all");
        pear::cli::run_unstage(unstage_paths, unstage_all);
    });
    status_cmd->callback([]() { pear::cli::run_status(); });
    ls_cmd->callback([]() { pear::cli::run_ls(); });
    deinit_cmd->callback([]() { pear::cli::run_deinit(); });
    disconnect_cmd->callback([]() { pear::cli::run_disconnect(); });

    // Обнаруживаем существующий workspace и загружаем конфиг
    try {
        auto ws = pear::storage::Workspace::discover(".");
        pear::g_workspace_root = ws.get_root();
        pear::g_db = std::make_shared<FakeDatabaseFacade>();
        pear::g_fs = std::make_shared<FakeFilesystemFacade>();
        auto loaded_addr = load_listen_address(pear::g_workspace_root);
        if (!loaded_addr.empty()) {
            pear::g_listen_address = loaded_addr;
        }
        start_node_if_needed();
    } catch (...) {}

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    try {
        app.require_subcommand(1);
        CLI11_PARSE(app, argc, argv);
    } catch (const CLI::ParseError& e) {
        return app.exit(e);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    if (pear::g_node_running) {
        while (pear::g_node_running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    return 0;
}