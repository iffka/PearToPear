#ifndef PEAR_FILESYSTEM_WORKSPACE_HPP
#define PEAR_FILESYSTEM_WORKSPACE_HPP

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace pear::storage {

namespace fs = std::filesystem;

struct Workspace {
private:
    fs::path m_root;
    fs::path m_peer_dir;
    fs::path m_obj_dir;
    fs::path m_meta_dir;

    explicit Workspace(fs::path root);

    fs::path create_empty_file(const std::string& filename);

public:
    // generate_object_id пока что просто для примера
    static std::string generate_object_id(const fs::path& path_to_local_file);

    // getters:
    const fs::path& get_root() const;
    const fs::path& get_peer_dir() const;
    const fs::path& get_obj_dir() const;
    const fs::path& get_meta_dir() const;

    static std::optional<fs::path> find_peer_root(const fs::path& start_dir);
    static Workspace init(const fs::path& root = fs::current_path());
    static Workspace discover(const fs::path& start_dir = fs::current_path());

    fs::path create_objectfile(const fs::path& path_to_local_file);
    void delete_objectfile(const std::string& id);

    void create_all_empty_files(const std::vector<std::string>& names_to_meta_files);
};

}  // namespace pear::storage

#endif  // PEAR_FILESYSTEM_WORKSPACE_HPP