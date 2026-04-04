#ifndef PEAR_DB_INIT_HPP
#define PEAR_DB_INIT_HPP

#include <pear/db/db.hpp>
#include <pear/fs/workspace.hpp>

namespace pear::db {

inline Database open_db(const pear::storage::Workspace& ws) {
    auto db_path = ws.get_meta_dir() / "db.sqlite";
    Database db(db_path);
    db.init();
    return db;
}

}

#endif
