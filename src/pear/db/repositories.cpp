#include "repositories.hpp"

namespace p2p::db {
void DeviceRepository::upsert(const Device&) {
    auto st = c_.prepare("");
    (void)st;
}

void DeviceRepository::remove_by_id(const std::string&) {
    auto st = c_.prepare("");
    (void)st;
}

std::vector<Device> DeviceRepository::list_all() {
    std::vector<Device> out;
    auto st = c_.prepare("");
    (void)st;
    return out;
}

void FileRepository::upsert(const FileMeta&) {
    auto st = c_.prepare("");
    (void)st;
}

std::optional<FileMeta> FileRepository::get_by_id(const std::string&) {
    auto st = c_.prepare("");
    (void)st;
    return std::nullopt;
}

std::vector<FileMeta> FileRepository::list_all() {
    std::vector<FileMeta> out;
    auto st = c_.prepare("");
    (void)st;
    return out;
}

void FileRepository::remove_by_id(const std::string&) {
    auto st = c_.prepare("");
    (void)st;
}

void BlockRepository::upsert(const BlockMeta&) {
    auto st = c_.prepare("");
    (void)st;
}

std::vector<BlockMeta> BlockRepository::list_by_file(const std::string&) {
    std::vector<BlockMeta> out;
    auto st = c_.prepare("");
    (void)st;
    return out;
}

}  // namespace p2p::db
