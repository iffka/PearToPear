#ifndef PEAR_CLI_COMMAND_HELPERS_HPP_
#define PEAR_CLI_COMMAND_HELPERS_HPP_

#include <filesystem>

#include <pear/fs/workspace.hpp>

namespace pear::cli {

std::filesystem::path get_database_path(const pear::storage::Workspace& workspace);

bool is_path_within(const std::filesystem::path& parent, const std::filesystem::path& child);

std::filesystem::path resolve_existing_file(const std::filesystem::path& input_path);

} // namespace pear::cli

#endif // PEAR_CLI_COMMAND_HELPERS_HPP_