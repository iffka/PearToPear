#include <pear/cli/commands.hpp>

#include <pear/db/sqlite_database.hpp>
#include <pear/fs/workspace.hpp>

#include "command_helpers.hpp"

#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace {

constexpr const char* GRUSHA = "🍐 ";

} // namespace

namespace pear::cli {

void run_init(const std::filesystem::path& workspace_path) {
#ifdef PEAR_DEBUG
    std::cout << "[DEBUG] run_init called\n";
    std::cout << "[DEBUG] workspace_path: " << workspace_path << '\n';
#endif

    pear::storage::Workspace workspace = pear::storage::Workspace::init(workspace_path);
    [[maybe_unused]] pear::db::SqliteDatabase database(get_database_path(workspace));
    std::cout << GRUSHA << "initialized workspace at " << workspace.get_root() << '\n';

}

void run_deinit() {
#ifdef PEAR_DEBUG
    std::cout << "[DEBUG] run_deinit called\n";
#endif
}

void run_connect(const std::string& gu_address, const std::string& listen_address, bool is_main) {
#ifdef PEAR_DEBUG
    std::cout << "[DEBUG] run_connect called\n";
    std::cout << "[DEBUG] gu_address: " << gu_address << '\n';
    std::cout << "[DEBUG] listen_address: " << listen_address << '\n';
    std::cout << "[DEBUG] is_main: " << std::boolalpha << is_main << '\n';
#endif
}

void run_disconnect() {
#ifdef PEAR_DEBUG
    std::cout << "[DEBUG] run_disconnect called\n";
#endif
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
}

void run_update() {
#ifdef PEAR_DEBUG
    std::cout << "[DEBUG] run_update called\n";
#endif
}

void run_ls() {
#ifdef PEAR_DEBUG
    std::cout << "[DEBUG] run_ls called\n";
#endif
}

void run_push() {
#ifdef PEAR_DEBUG
    std::cout << "[DEBUG] run_push called\n";
#endif
}

void run_pull(const std::vector<std::string>& targets) {
#ifdef PEAR_DEBUG
    std::cout << "[DEBUG] run_pull called\n";

    for (const auto& target : targets) {
        std::cout << "[DEBUG] target: " << target << '\n';
    }
#endif
}

void run_status() {
#ifdef PEAR_DEBUG
    std::cout << "[DEBUG] run_status called\n";
#endif
}

} // namespace pear::cli