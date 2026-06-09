#ifndef PEAR_DEMON_GU_TRANSITION_HPP
#define PEAR_DEMON_GU_TRANSITION_HPP

#include <cstdint>
#include <filesystem>
#include <string>

namespace pear::demon {

struct GuTransitionState {
    uint64_t device_id = 0;
    std::string local_address;
    std::string master_address;
    bool is_local_gu = false;
};

GuTransitionState get_gu_transition_state(const std::filesystem::path& workspace_root);

void promote_local_node_to_gu(const std::filesystem::path& workspace_root);

void switch_to_gu(const std::filesystem::path& workspace_root, const std::string& new_gu_address);

} // namespace pear::demon

#endif // PEAR_DEMON_GU_TRANSITION_HPP