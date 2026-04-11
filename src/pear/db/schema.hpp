#ifndef PEAR_DB_SCHEMA_HPP
#define PEAR_DB_SCHEMA_HPP

#include "sqlite.hpp"

// Схема БД MVP — таблицы WAL, files, devices, local_config.
// Вызывается один раз из конструктора SqliteDatabase.

namespace pear::db {

// Создаёт все нужные таблицы (идемпотентно).
void ensure_schema(Connection& c);

}  // namespace pear::db

#endif  // PEAR_DB_SCHEMA_HPP
