#ifndef PEAR_DB_SCHEMA_HPP
#define PEAR_DB_SCHEMA_HPP

#include "sqlite.hpp"

// Вызывается один раз из конструктора SqliteDatabase

namespace pear::db {

// Создаёт все нужные таблицы 
void ensure_schema(Connection& c);

}  // namespace pear::db

#endif  // PEAR_DB_SCHEMA_HPP
