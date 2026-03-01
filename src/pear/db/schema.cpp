#include "schema.hpp"

namespace p2p::db {

std::string bootstrap_sql() {
    return R"sql(
CREATE TABLE IF NOT EXISTS schema_version(
  version INTEGER NOT NULL
);
)sql";
}

std::string schema_sql() {
    return std::string{};
}

static int current_version(Connection& c) {
    c.exec(bootstrap_sql());
    auto st = c.prepare("SELECT version FROM schema_version LIMIT 1;");
    if (st.step()) return static_cast<int>(st.col_i64(0));
    return 0;
}

static void set_version(Connection& c, int v) {
    c.exec("DELETE FROM schema_version;");
    auto st = c.prepare("INSERT INTO schema_version(version) VALUES(?1);");
    st.bind(1, v);
    st.run();
}

void ensure_schema(Connection& c) {
    const int v = current_version(c);
    if (v == 0) {
        c.begin();
        try {
            const auto s = schema_sql();
            if (!s.empty()) c.exec(s);
            set_version(c, 3);
            c.commit();
        } catch (...) {
            c.rollback();
            throw;
        }
    }
}

}  // namespace p2p::db
