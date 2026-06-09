#ifndef PEAR_DEMON_GU_TRANSITION_HPP
#define PEAR_DEMON_GU_TRANSITION_HPP

#include <filesystem>

namespace pear::demon {

bool recover_gu_from_replicas(const std::filesystem::path& workspace_root);

} // namespace pear::demon

#endif // PEAR_DEMON_GU_TRANSITION_HPP