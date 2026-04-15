#ifndef DEMON_HPP
#define DEMON_HPP

#include <filesystem>
#include <string>

namespace pear::demon {

void spawn(const std::filesystem::path& workspace_root, const std::string& listen_address, bool is_main);

void kill(const std::filesystem::path& workspace_root);

bool is_alive(const std::filesystem::path& workspace_root);

} // namespace pear::demon

#endif