#pragma once

#include <cstdio>
#include <string>

namespace top::save_recovery {

enum class CommitResult {
    Durable,
    RolledBack,
    VisibleButUncertain
};

using DirectorySync = bool (*)(const std::string& path, void* context);

// Read the first bounded save line, preferring the active file and then its
// adjacent .bak copy. The recovered flag is true only for the fallback.
bool read(const std::string& path, std::string& line, bool& recovered);

// Atomically replace the adjacent .bak with a durable copy of the current
// save. A missing current save is a successful no-op.
bool preservePrevious(const std::string& path);

// Replace a primary save from a same-directory temporary file. If the parent
// directory cannot be synced after rename, restore the previous valid backup
// (or remove the first save) so callers can roll back matching memory state.
// VisibleButUncertain means recovery itself failed and the caller must keep
// memory aligned with the currently visible new save rather than undoing it.
CommitResult commit(const std::string& temporaryPath, const std::string& path,
                    DirectorySync sync = nullptr, void* context = nullptr);

// Append a CRC1 trailer to an open, currently unterminated save line.
bool appendChecksum(FILE* file);

} // namespace top::save_recovery
