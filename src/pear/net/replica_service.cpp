#include "pear/net/replica_service.hpp"

#include "pear/db/sqlite_database.hpp"
#include "pear/net/db_types.hpp"

#include <stdexcept>
#include <utility>
#include <vector>

namespace pear::net {
namespace {

WalEntryInfo parseProtoWalEntry(const WalEntry& proto_entry) {
    WalEntryInfo entry;
    entry.seq_id = proto_entry.seq_id();
    entry.timestamp = proto_entry.timestamp();
    entry.entry_view_number = proto_entry.entry_view_number();
    entry.op_type = static_cast<WalOpTypeInfo>(proto_entry.op_type());

    if (proto_entry.has_file_update()) {
        entry.file.path = proto_entry.file_update().path();
        entry.file.object_hash = proto_entry.file_update().object_hash();
        entry.file.version = proto_entry.file_update().version();
        entry.file.owner_device_id = proto_entry.file_update().owner_device_id();
        entry.file.read_only = proto_entry.file_update().read_only();
        return entry;
    }

    if (proto_entry.has_file_delete()) {
        entry.file_delete.path = proto_entry.file_delete().path();
        entry.file_delete.version = proto_entry.file_delete().version();
        entry.file_delete.owner_device_id = proto_entry.file_delete().owner_device_id();
        return entry;
    }

    if (proto_entry.has_device_update()) {
        entry.device.device_id = proto_entry.device_update().device_id();
        entry.device.address = proto_entry.device_update().address();
        return entry;
    }

    if (proto_entry.has_object_owner_update()) {
        entry.object_owner.object_hash = proto_entry.object_owner_update().object_hash();
        entry.object_owner.owner_device_id = proto_entry.object_owner_update().owner_device_id();
        return entry;
    }

    if (proto_entry.has_object_owner_delete()) {
        entry.object_owner.object_hash = proto_entry.object_owner_delete().object_hash();
        entry.object_owner.owner_device_id = proto_entry.object_owner_delete().owner_device_id();
    }

    return entry;
}

} // namespace

ReplicaServiceImpl::ReplicaServiceImpl(std::shared_ptr<pear::db::SqliteDatabase> db) : db_(std::move(db)) {}

grpc::Status ReplicaServiceImpl::Prepare(
    grpc::ServerContext* /*ctx*/,
    const PrepareRequest* req,
    PrepareResponse* resp
) {
    std::unique_lock lock(db_mutex_);

    try {
        VrStateInfo state = db_->getVrState();

        if (req->view_number() < state.view_number) {
            resp->set_success(false);
            resp->set_view_number(state.view_number);
            resp->set_last_seq_id(db_->getLastSeqId());
            resp->set_error_message("prepare from old view");
            return grpc::Status::OK;
        }

        std::vector<WalEntryInfo> entries;
        entries.reserve(static_cast<std::size_t>(req->entries_size()));

        for (int i = 0; i < req->entries_size(); ++i) {
            WalEntryInfo entry = parseProtoWalEntry(req->entries(i));

            if (entry.seq_id == 0) {
                throw std::runtime_error("prepare entry has empty seq_id");
            }

            entries.push_back(std::move(entry));
        }

        if (!entries.empty()) {
            db_->applyWalEntries(entries);
        }

        if (req->view_number() > state.view_number) {
            state.view_number = req->view_number();
            state.last_normal_view = req->view_number();
            state.status = ReplicaStatusInfo::kReplicaNormal;
            db_->setVrState(state);
        }

        const uint64_t last_seq_id = db_->getLastSeqId();

        if (req->commit_number() > last_seq_id) {
            throw std::runtime_error("prepare commit number is ahead of local log");
        }

        if (req->commit_number() > state.commit_number) {
            db_->setCommitNumber(req->commit_number());
        }

        resp->set_success(true);
        resp->set_view_number(db_->getVrState().view_number);
        resp->set_last_seq_id(last_seq_id);
    } catch (const std::exception& error) {
        resp->set_success(false);
        resp->set_view_number(db_->getVrState().view_number);
        resp->set_last_seq_id(db_->getLastSeqId());
        resp->set_error_message(error.what());
    }

    return grpc::Status::OK;
}

grpc::Status ReplicaServiceImpl::Commit(
    grpc::ServerContext* /*ctx*/,
    const CommitRequest* req,
    CommitResponse* resp
) {
    std::unique_lock lock(db_mutex_);

    try {
        VrStateInfo state = db_->getVrState();

        if (req->view_number() < state.view_number) {
            resp->set_success(false);
            resp->set_error_message("commit from old view");
            return grpc::Status::OK;
        }

        const uint64_t last_seq_id = db_->getLastSeqId();

        if (req->commit_number() > last_seq_id) {
            throw std::runtime_error("commit number is ahead of local log");
        }

        if (req->view_number() > state.view_number) {
            state.view_number = req->view_number();
            state.last_normal_view = req->view_number();
            state.status = ReplicaStatusInfo::kReplicaNormal;
            db_->setVrState(state);
        }

        db_->setCommitNumber(req->commit_number());

        resp->set_success(true);
    } catch (const std::exception& error) {
        resp->set_success(false);
        resp->set_error_message(error.what());
    }

    return grpc::Status::OK;
}

grpc::Status ReplicaServiceImpl::GetReplicaState(
    grpc::ServerContext* /*ctx*/,
    const ReplicaStateRequest* /*req*/,
    ReplicaStateResponse* resp
) {
    std::unique_lock lock(db_mutex_);

    try {
        const VrStateInfo state = db_->getVrState();
        const uint64_t device_id = db_->getDeviceId();

        resp->set_success(true);
        resp->set_device_id(device_id);

        if (device_id != 0) {
            resp->set_address(db_->getDeviceAddress(device_id));
        }

        resp->set_view_number(state.view_number);
        resp->set_commit_number(state.commit_number);
        resp->set_last_normal_view(state.last_normal_view);
        resp->set_replica_status(static_cast<uint64_t>(state.status));
        resp->set_last_seq_id(db_->getLastSeqId());
    } catch (const std::exception& error) {
        resp->set_success(false);
        resp->set_error_message(error.what());
    }

    return grpc::Status::OK;
}

} // namespace pear::net