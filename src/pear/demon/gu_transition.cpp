#include "pear/demon/gu_transition.hpp"

#include "pear/db/sqlite_database.hpp"
#include "pear/fs/workspace.hpp"

#include <stdexcept>

namespace pear::demon {
namespace {

std::filesystem::path get_database_path(const pear::storage::Workspace& workspace) {
    return workspace.get_meta_dir() / "peer.db";
}

} // namespace

GuTransitionState get_gu_transition_state(const std::filesystem::path& workspace_root) {
    const auto workspace = pear::storage::Workspace::discover(workspace_root);
    pear::db::SqliteDatabase database(get_database_path(workspace));

    GuTransitionState state;
    state.device_id = database.getDeviceId();
    state.master_address = database.getMasterAddress();

    if (state.device_id != 0) {
        state.local_address = database.getDeviceAddress(state.device_id);
    }

    state.is_local_gu = !state.local_address.empty() && state.local_address == state.master_address;

    return state;
}

void promote_local_node_to_gu(const std::filesystem::path& workspace_root) {
    const auto workspace = pear::storage::Workspace::discover(workspace_root);
    pear::db::SqliteDatabase database(get_database_path(workspace));

    const uint64_t device_id = database.getDeviceId();

    if (device_id == 0) {
        throw std::runtime_error("device id is unknown");
    }

    const std::string local_address = database.getDeviceAddress(device_id);

    if (local_address.empty()) {
        throw std::runtime_error("local device address is unknown");
    }

    database.setMasterAddress(local_address);
}

void switch_to_gu(const std::filesystem::path& workspace_root, const std::string& new_gu_address) {
    if (new_gu_address.empty()) {
        throw std::runtime_error("new gu address is empty");
    }

    const auto workspace = pear::storage::Workspace::discover(workspace_root);
    pear::db::SqliteDatabase database(get_database_path(workspace));

    database.setMasterAddress(new_gu_address);
}

} // namespace pear::demon