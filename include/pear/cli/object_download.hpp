#ifndef PEAR_CLI_OBJECT_DOWNLOAD_HPP_
#define PEAR_CLI_OBJECT_DOWNLOAD_HPP_

#include <cstdint>
#include <filesystem>

#include "pear/db/sqlite_database.hpp"
#include "pear/net/db_types.hpp"

namespace pear::cli {

void download_object_from_owners(pear::db::SqliteDatabase& database, const pear::net::FileUpdateInfo& file, uint64_t device_id, const std::filesystem::path& destination_path);

} // namespace pear::cli

#endif // PEAR_CLI_OBJECT_DOWNLOAD_HPP_