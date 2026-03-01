#ifndef P2P_DB_SCHEMA_HPP
#define P2P_DB_SCHEMA_HPP

#include <string>

#include "sqlite.hpp"

namespace p2p::db {

constexpr std::string_view kSchemaVersionTable = "schema_version";

std::string schema_sql();
std::string bootstrap_sql();
void ensure_schema(Connection& c);

}  // namespace p2p::db

#endif  // P2P_DB_SCHEMA_HPP
