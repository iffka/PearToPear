#include <pear/fs/workspace.hpp>

#include <fstream>
#include <stdexcept>

namespace {
namespace fs = std::filesystem;
std::optional<fs::path> find_peer_root(const fs::path& start_dir) {
    fs::path current_dir = start_dir;
    while (!fs::exists(current_dir / ".peer")) {
        if (current_dir == current_dir.parent_path()) {
            return std::nullopt;
        }
        current_dir = current_dir.parent_path();
    }
    return current_dir;
}

} // anonymous namespace

namespace pear::storage {

Workspace::Workspace(fs::path root)
    : m_root(std::move(root)),
      m_peer_dir(m_root / ".peer"),
      m_obj_dir(m_peer_dir / "obj"),
      m_meta_dir(m_peer_dir / "meta") {}

fs::path Workspace::create_empty_file(const std::string& filename) {
    fs::path file_path = m_root / (filename + ".empty");
    if (fs::exists(file_path))
        return file_path;
    std::ofstream(file_path).close();
    fs::permissions(
        file_path,
        fs::perms::owner_read | fs::perms::group_read | fs::perms::others_read,
        fs::perm_options::replace
    );
    return file_path;
}

// generate_object_id пока что просто для примера
std::string Workspace::generate_object_id(const fs::path& path_to_local_file) {
    return path_to_local_file.filename().string();
}

// getters:
const fs::path& Workspace::get_root() const {
    return m_root;
}
const fs::path& Workspace::get_peer_dir() const {
    return m_peer_dir;
}
const fs::path& Workspace::get_obj_dir() const {
    return m_obj_dir;
}
const fs::path& Workspace::get_meta_dir() const {
    return m_meta_dir;
}

Workspace Workspace::init(const fs::path& root) {
    if (find_peer_root(root))
        throw std::runtime_error("Workspace already initialized");
    Workspace tmp_workspace(root);
    fs::create_directories(tmp_workspace.get_peer_dir());
    fs::create_directories(tmp_workspace.get_obj_dir());
    fs::create_directories(tmp_workspace.get_meta_dir());
    return tmp_workspace;
}

Workspace Workspace::discover(const fs::path& start_dir) {
    auto root = find_peer_root(start_dir);
    if (!root)
        throw std::runtime_error("No workspace found");
    return Workspace(*root);
}

fs::path Workspace::create_objectfile(const std::string& object_name, const fs::path& path_to_source_file) {
    if (!fs::exists(path_to_source_file) || !fs::is_regular_file(path_to_source_file)) {
        throw std::runtime_error("Invalid file");
    }
    fs::path object_path = m_obj_dir / object_name;
    fs::copy_file(path_to_source_file, object_path, fs::copy_options::overwrite_existing);
    return object_path;
}

fs::path Workspace::get_objectfile_path(const std::string& object_name) const {
    fs::path object_path = m_obj_dir / object_name;
    if (!fs::exists(object_path) || !fs::is_regular_file(object_path)) {
        throw std::runtime_error("Invalid object file");
    }
    return object_path;
}

void Workspace::delete_objectfile(const std::string& object_name) {
    fs::path object_path = m_obj_dir / object_name;
    if (!fs::exists(object_path) || !fs::is_regular_file(object_path))
        throw std::runtime_error("Invalid object file");
    fs::remove(object_path);
}

void Workspace::create_all_empty_files(const std::vector<std::string>& names_to_meta_files) {
    for (const auto& path : names_to_meta_files) {
        create_empty_file(path);
    }
}

std::vector<std::string> Workspace::get_list_object_ids() const {
    std::vector<std::string> object_ids;
    for (const auto& entry : fs::directory_iterator(m_obj_dir)) {
        if (entry.is_regular_file()) {
            object_ids.push_back(entry.path().filename().string());
        }
    }
    return object_ids;
}

bool Workspace::has_objectfile(const std::string& object_name) const {
    fs::path object_path = m_obj_dir / object_name;
    return fs::is_regular_file(object_path);
}

}  // namespace pear::storage