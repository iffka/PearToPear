#ifndef PEAR_NET_FS_CONTRACT_HPP_
#define PEAR_NET_FS_CONTRACT_HPP_

#include <cstdint>
#include <filesystem>
#include <string>

namespace pear::net {

class FilesystemFacade {
public:
    virtual ~FilesystemFacade() = default;

    virtual std::filesystem::path getObjectPath(const std::string& file_id, uint64_t version) = 0;
    virtual void incrementDownloadCounter(const std::string& file_id, uint64_t version) = 0;
    virtual void decrementDownloadCounter(const std::string& file_id, uint64_t version) = 0;
    virtual void tryDeleteOldVersion(const std::string& file_id, uint64_t version) = 0;
    virtual std::filesystem::path storeObject(const std::string& file_id, uint64_t version, const std::filesystem::path& source_file) = 0;
};

} // namespace pear::net

#endif // PEAR_NET_FS_CONTRACT_HPP_