#include <pear/net/master_service.hpp>

#include <utility>

namespace pear::net {

MasterServiceImpl::MasterServiceImpl(std::shared_ptr<pear::db::SqliteDatabase> db) : db_(std::move(db)) {}

grpc::Status MasterServiceImpl::RegisterDevice(grpc::ServerContext* /*ctx*/, const RegisterRequest* req, RegisterResponse* resp) {
    try {
        uint64_t new_id = db_->registerDevice(req->address());

        resp->set_success(true);
        resp->set_assigned_device_id(new_id);

        auto full_wal = db_->getWalEntriesSince(0);
        for (const auto& entry : full_wal) {
            auto* pe = resp->add_full_wal();
            pe->set_seq_id(entry.seq_id);
            pe->set_timestamp(entry.timestamp);
            pe->set_op_type(static_cast<WalOpType>(entry.op_type));

            if (entry.op_type == 0) {
                auto* fu = pe->mutable_file_update();
                fu->set_file_id(entry.file.file_id);
                fu->set_name(entry.file.name);
                fu->set_version(entry.file.version);
                fu->set_owner_device_id(entry.file.owner_device_id);
            } else if (entry.op_type == 1) {
                auto* du = pe->mutable_device_update();
                du->set_device_id(entry.device.device_id);
                du->set_address(entry.device.address);
            }
        }
    } catch (const std::exception& e) {
        resp->set_success(false);
        resp->set_error_message(e.what());
    }

    return grpc::Status::OK;
}

grpc::Status MasterServiceImpl::UpdateDB(grpc::ServerContext* /*ctx*/, const UpdateDBRequest* req, UpdateDBResponse* resp) {
    try {
        auto entries = db_->getWalEntriesSince(req->last_seq_id());

        for (const auto& entry : entries) {
            auto* pe = resp->add_entries();
            pe->set_seq_id(entry.seq_id);
            pe->set_timestamp(entry.timestamp);
            pe->set_op_type(static_cast<WalOpType>(entry.op_type));

            if (entry.op_type == 0) {
                auto* fu = pe->mutable_file_update();
                fu->set_file_id(entry.file.file_id);
                fu->set_name(entry.file.name);
                fu->set_version(entry.file.version);
                fu->set_owner_device_id(entry.file.owner_device_id);
            } else if (entry.op_type == 1) {
                auto* du = pe->mutable_device_update();
                du->set_device_id(entry.device.device_id);
                du->set_address(entry.device.address);
            }
        }

        resp->set_success(true);
    } catch (const std::exception& e) {
        resp->set_success(false);
        resp->set_error_message(e.what());
    }

    return grpc::Status::OK;
}

grpc::Status MasterServiceImpl::PushWAL(grpc::ServerContext* /*ctx*/, const PushWALRequest* req, PushWALResponse* resp) {
    try {
        for (int i = 0; i < req->entries_size(); ++i) {
            const auto& entry = req->entries(i);

            WalEntryInfo info;
            info.seq_id = 0;
            info.timestamp = entry.timestamp();
            info.op_type = static_cast<int>(entry.op_type());

            if (entry.has_file_update()) {
                info.file.file_id = entry.file_update().file_id();
                info.file.name = entry.file_update().name();
                info.file.version = entry.file_update().version();
                info.file.owner_device_id = entry.file_update().owner_device_id();
            } else if (entry.has_device_update()) {
                info.device.device_id = entry.device_update().device_id();
                info.device.address = entry.device_update().address();
            }

            uint64_t new_seq = db_->addWalEntry(info);
            resp->add_assigned_seq_ids(new_seq);
        }

        resp->set_success(true);
    } catch (const std::exception& e) {
        resp->set_success(false);
        resp->set_error_message(e.what());
    }

    return grpc::Status::OK;
}

} // namespace pear::net