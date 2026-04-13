#include "command_helpers.hpp"

#include <algorithm>
#include <stdexcept>

namespace pear::cli {

namespace fs = std::filesystem;

fs::path get_database_path(const pear::storage::Workspace& workspace) {
    return workspace.get_meta_dir() / "peer.db";
}

bool is_path_within(const fs::path& parent, const fs::path& child) {
    fs::path normalized_parent = fs::weakly_canonical(parent);
    fs::path normalized_child = fs::weakly_canonical(child);

    auto mismatch = std::mismatch(
        normalized_parent.begin(),
        normalized_parent.end(),
        normalized_child.begin(),
        normalized_child.end()
    );

    return mismatch.first == normalized_parent.end();
}

fs::path resolve_existing_file(const fs::path& input_path) {
    fs::path resolved_path = input_path;
    if (!resolved_path.is_absolute()) {
        resolved_path = fs::absolute(resolved_path);
    }
    resolved_path = fs::weakly_canonical(resolved_path);
    if (!fs::exists(resolved_path)) {
        throw std::runtime_error("Path does not exist: " + resolved_path.string());
    }
    if (!fs::is_regular_file(resolved_path)) {
        throw std::runtime_error("Path is not a regular file: " + resolved_path.string());
    }
    return resolved_path;
}

} // namespace pear::cli