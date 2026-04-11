#include <pear/cli/commands.hpp>

#include <iostream>

namespace pear::cli {

void run_init(const std::filesystem::path& workspace_path) {
#ifdef PEAR_DEBUG
    std::cout << "[DEBUG] run_init called\n";
    std::cout << "[DEBUG] workspace_path: " << workspace_path << '\n';
#endif
}

void run_deinit() {
#ifdef PEAR_DEBUG
    std::cout << "[DEBUG] run_deinit called\n";
#endif
}

void run_connect(const std::string& repo_id, bool is_main) {
#ifdef PEAR_DEBUG
    std::cout << "[DEBUG] run_connect called\n";
    std::cout << "[DEBUG] repo_id: " << repo_id << '\n';
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