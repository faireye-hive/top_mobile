#pragma once
#include <string>
namespace top::save_paths {
inline std::string character(const std::string& root, int id) {
    if (root.empty()) return {};
    return root + "/user/offline-save-" + std::to_string(id) + ".txt";
}
} // namespace top::save_paths
