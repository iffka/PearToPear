#include "pear/demon/gu_transition.hpp"

#include "pear/db/sqlite_database.hpp"
#include "pear/fs/workspace.hpp"
#include "pear/net/pear_transport.hpp"
#include "pear/net/transport_registry.hpp"

#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace pear::demon {
namespace {

std::filesystem::path get_database_path(const pear::storage::Workspace& workspace) {
    return workspace.get_meta_dir() / "peer.db";
}

bool is_better_replica(const pear::net::ReplicaStateInfo& candidate, const pear::net::ReplicaStateInfo& current) {
    if (candidate.last_seq_id != current.last_seq_id) {
        return candidate.last_seq_id > current.last_seq_id;
    }

    if (candidate.vr_state.view_number != current.vr_state.view_number) {
        return candidate.vr_state.view_number > current.vr_state.view_number;
    }

    return candidate.device_id < current.device_id;
}

} // namespace

bool recover_gu_from_replicas(const std::filesystem::path& workspace_root) {
    pear::storage::Workspace workspace = pear::storage::Workspace::discover(workspace_root);
    pear::db::SqliteDatabase database(get_database_path(workspace));

    const uint64_t local_device_id = database.getDeviceId();

    if (local_device_id == 0) {
        return false;
    }

    pear::net::ReplicaStateInfo best_state;
    best_state.device_id = local_device_id;
    best_state.address = database.getDeviceAddress(local_device_id);
    best_state.vr_state = database.getVrState();
    best_state.last_seq_id = database.getLastSeqId();

    if (best_state.address.empty()) {
        return false;
    }

    auto& transport = pear::net::transport();
    const auto devices = database.getAllDeviceAddresses();

    for (const auto& [device_id, address] : devices) {
        if (address.empty() || address == best_state.address) {
            continue;
        }

        try {
            pear::net::ReplicaStateInfo state = transport.getReplicaState(address, local_device_id);

            if (state.address.empty()) {
                state.address = address;
            }

            if (is_better_replica(state, best_state)) {
                best_state = state;
            }
        } catch (const std::exception& error) {
            std::cerr << "warning: failed to get replica state from " << address << ": " << error.what() << '\n';
        }
    }

    if (best_state.address.empty()) {
        return false;
    }

    database.setMasterAddress(best_state.address);

    pear::net::VrStateInfo local_state = database.getVrState();

    if (best_state.vr_state.view_number >= local_state.view_number) {
        local_state.view_number = best_state.vr_state.view_number + 1;
        local_state.last_normal_view = local_state.view_number;
        local_state.status = pear::net::ReplicaStatusInfo::kReplicaNormal;
        database.setVrState(local_state);
    }

    return true;
}

} // namespace pear::demon