#ifndef PEAR_NET_REMOTE_CLIENT_HPP
#define PEAR_NET_REMOTE_CLIENT_HPP

#include <cstdint>
#include <string>
#include <vector>

#include "db_contract.hpp"

namespace pear::net {

class RemoteClient {
public:
    static uint64_t RegisterDevice(const std::string& gu_address, const std::string& my_address);

    static std::vector<WalEntryInfo> UpdateDB(const std::string& gu_address,
                                               uint64_t last_seq_id,
                                               uint64_t device_id);

    // Отправляет записи, возвращает true и заполняет assigned_seq_ids
    static bool PushWAL(const std::string& gu_address,
                        uint64_t device_id,
                        const std::vector<WalEntryInfo>& entries,
                        std::vector<uint64_t>& out_assigned_seq_ids);

    static void DownloadFile(const std::string& vu_address,
                             const std::string& file_id,
                             uint64_t version,
                             uint64_t requester_device_id,
                             const std::string& destination_path);
};

} // namespace pear::net

#endif