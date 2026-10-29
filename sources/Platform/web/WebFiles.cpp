#include <cstdio>
#include <cerrno>
#include <emscripten.h>
#include <unordered_map>
#include <string>

// Both stdio and libc++ filebuf use fopen. Download before opening so the
// existing binary/text readers and case-insensitive path resolver stay shared.
EM_ASYNC_JS(int, ensureAsset, (const char* path), {
    return await Module.topEnsureAsset(UTF8ToString(path));
});
EM_JS(int, needsAsset, (const char* path), {
    return Module.topNeedsAsset(UTF8ToString(path)) ? 1 : 0;
});
EM_JS(int, pinAsset, (const char* path), {
    return Module.topPinAsset(UTF8ToString(path));
});
EM_JS(void, unpinAsset, (const char* path), {
    Module.topUnpinAsset(UTF8ToString(path));
});
namespace { std::unordered_map<FILE*, std::string> openAssets; }

extern "C" FILE* __real_fopen(const char*, const char*);
extern "C" int __real_fclose(FILE*);
extern "C" FILE* __wrap_fopen(const char* path, const char* mode) {
    const bool pinned = path && mode && mode[0] == 'r' && pinAsset(path);
    if (path && mode && mode[0] == 'r' && needsAsset(path) && !ensureAsset(path)) {
        if (pinned) unpinAsset(path);
        errno = EIO;
        return nullptr;
    }
    FILE* file = __real_fopen(path, mode);
    if (pinned) {
        if (file) openAssets.emplace(file, path);
        else unpinAsset(path);
    }
    return file;
}
extern "C" int __wrap_fclose(FILE* file) {
    const int result = __real_fclose(file);
    const auto found = openAssets.find(file);
    if (found != openAssets.end()) {
        const std::string path = std::move(found->second);
        openAssets.erase(found);
        unpinAsset(path.c_str());
    }
    return result;
}
