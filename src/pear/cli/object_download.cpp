#include "pear/cli/object_download.hpp"

#include "pear/fs/hash.hpp"
#include "pear/net/transport_registry.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <fstream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace pear::cli {
namespace {

constexpr uint64_t kChunkSize = 4 * 1024 * 1024;
constexpr std::size_t kMaxDownloadThreads = 8;

std::vector<std::string> collect_owner_addresses(pear::db::SqliteDatabase& database, const pear::net::FileUpdateInfo& file) {
    std::vector<std::string> owner_addresses;

    const std::string primary_owner_address = database.getDeviceAddress(file.owner_device_id);
    if (!primary_owner_address.empty()) {
        owner_addresses.push_back(primary_owner_address);
    }

    for (const auto& owner_address : database.getObjectOwnerAddresses(file.object_hash)) {
        if (!owner_address.empty() && std::find(owner_addresses.begin(), owner_addresses.end(), owner_address) == owner_addresses.end()) {
            owner_addresses.push_back(owner_address);
        }
    }

    if (owner_addresses.empty()) {
        throw std::runtime_error("no known owners for object " + file.object_hash);
    }

    return owner_addresses;
}

uint64_t get_object_size_from_any_owner(const std::vector<std::string>& owner_addresses, const std::string& object_hash, uint64_t device_id) {
    std::string last_error;

    for (const auto& owner_address : owner_addresses) {
        try {
            return pear::net::transport().getObjectSize(owner_address, object_hash, device_id);
        } catch (const std::exception& error) {
            last_error = error.what();
        }
    }

    throw std::runtime_error("failed to get object size from all owners: " + last_error);
}

std::filesystem::path make_temp_download_dir(const std::filesystem::path& destination_path) {
    namespace fs = std::filesystem;

    const std::string temp_name = destination_path.filename().string() + ".download.parts." + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const fs::path temp_dir = destination_path.parent_path() / temp_name;

    fs::create_directories(temp_dir);
    return temp_dir;
}

void cleanup_temp(const std::filesystem::path& temp_dir) {
    std::error_code cleanup_error;
    std::filesystem::remove_all(temp_dir, cleanup_error);
}

std::vector<std::filesystem::path> make_chunk_paths(const std::filesystem::path& temp_dir, uint64_t object_size) {
    const uint64_t chunk_count = object_size == 0 ? 0 : (object_size + kChunkSize - 1) / kChunkSize;

    std::vector<std::filesystem::path> chunk_paths;
    chunk_paths.reserve(static_cast<std::size_t>(chunk_count));

    for (uint64_t chunk_index = 0; chunk_index < chunk_count; ++chunk_index) {
        chunk_paths.push_back(temp_dir / ("chunk." + std::to_string(chunk_index)));
    }

    return chunk_paths;
}

std::size_t get_download_thread_count(std::size_t chunk_count, std::size_t owner_count) {
    std::size_t thread_count = chunk_count;
    thread_count = std::min(thread_count, owner_count);

    const unsigned int hardware_threads = std::thread::hardware_concurrency();
    if (hardware_threads != 0) {
        thread_count = std::min(thread_count, static_cast<std::size_t>(hardware_threads));
    }

    thread_count = std::min(thread_count, kMaxDownloadThreads);
    return thread_count;
}

void download_one_chunk(const std::vector<std::string>& owner_addresses, const std::string& object_hash, uint64_t device_id, uint64_t object_size, const std::vector<std::filesystem::path>& chunk_paths, std::size_t chunk_index) {
    const uint64_t offset = static_cast<uint64_t>(chunk_index) * kChunkSize;
    const uint64_t size = std::min(kChunkSize, object_size - offset);

    std::string chunk_last_error;

    for (std::size_t attempt = 0; attempt < owner_addresses.size(); ++attempt) {
        const std::string& owner_address = owner_addresses[(chunk_index + attempt) % owner_addresses.size()];

        try {
            pear::net::transport().downloadFileRange(owner_address, object_hash, device_id, offset, size, chunk_paths[chunk_index].string());
            return;
        } catch (const std::exception& error) {
            chunk_last_error = error.what();
        }
    }

    throw std::runtime_error("failed to download object chunk: " + chunk_last_error);
}

std::vector<std::filesystem::path> download_chunks(const std::vector<std::string>& owner_addresses, const std::string& object_hash, uint64_t device_id, uint64_t object_size, const std::filesystem::path& temp_dir) {
    const std::vector<std::filesystem::path> chunk_paths = make_chunk_paths(temp_dir, object_size);
    const std::size_t thread_count = get_download_thread_count(chunk_paths.size(), owner_addresses.size());

    std::atomic<std::size_t> next_chunk_index {0};
    std::atomic<bool> has_error {false};
    std::mutex error_mutex;
    std::exception_ptr first_error = nullptr;

    auto save_first_error = [&]() {
        std::lock_guard<std::mutex> lock(error_mutex);
        if (!first_error) {
            first_error = std::current_exception();
        }
        has_error.store(true);
    };

    auto worker = [&]() {
        while (!has_error.load()) {
            const std::size_t chunk_index = next_chunk_index.fetch_add(1);
            if (chunk_index >= chunk_paths.size()) {
                return;
            }

            try {
                download_one_chunk(owner_addresses, object_hash, device_id, object_size, chunk_paths, chunk_index);
            } catch (...) {
                save_first_error();
                return;
            }
        }
    };

    std::vector<std::thread> threads;
    threads.reserve(thread_count);

    for (std::size_t thread_index = 0; thread_index < thread_count; ++thread_index) {
        threads.emplace_back(worker);
    }

    for (auto& thread : threads) {
        thread.join();
    }

    if (first_error) {
        std::rethrow_exception(first_error);
    }

    return chunk_paths;
}

void assemble_chunks(const std::vector<std::filesystem::path>& chunk_paths, const std::filesystem::path& assembled_path) {
    std::ofstream output(assembled_path, std::ios::binary);
    if (!output.is_open()) {
        throw std::runtime_error("failed to open assembled temp file");
    }

    for (const auto& chunk_path : chunk_paths) {
        std::ifstream input(chunk_path, std::ios::binary);
        if (!input.is_open()) {
            throw std::runtime_error("failed to open downloaded chunk");
        }

        output << input.rdbuf();
        if (!output.good()) {
            throw std::runtime_error("failed to write assembled temp file");
        }
    }

    output.close();
    if (!output.good()) {
        throw std::runtime_error("failed to close assembled temp file");
    }
}

void replace_destination_after_hash_check(const std::filesystem::path& assembled_path, const std::filesystem::path& destination_path, const std::string& expected_hash) {
    const std::string actual_hash = pear::storage::get_file_hash(assembled_path);
    if (actual_hash != expected_hash) {
        throw std::runtime_error("downloaded object hash mismatch");
    }

    std::error_code rename_error;
    std::filesystem::rename(assembled_path, destination_path, rename_error);
    if (rename_error) {
        throw std::runtime_error("failed to move assembled temp file");
    }
}

} // namespace

void download_object_from_owners(pear::db::SqliteDatabase& database, const pear::net::FileUpdateInfo& file, uint64_t device_id, const std::filesystem::path& destination_path) {
    const auto owner_addresses = collect_owner_addresses(database, file);
    const uint64_t object_size = get_object_size_from_any_owner(owner_addresses, file.object_hash, device_id);

    const auto temp_dir = make_temp_download_dir(destination_path);
    const auto assembled_path = temp_dir / "assembled";

    try {
        const auto chunk_paths = download_chunks(owner_addresses, file.object_hash, device_id, object_size, temp_dir);
        assemble_chunks(chunk_paths, assembled_path);
        replace_destination_after_hash_check(assembled_path, destination_path, file.object_hash);
        cleanup_temp(temp_dir);
    } catch (...) {
        cleanup_temp(temp_dir);
        throw;
    }
}

} // namespace pear::cli