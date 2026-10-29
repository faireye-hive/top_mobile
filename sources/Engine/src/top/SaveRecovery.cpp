#include "top/SaveRecovery.h"

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <sys/types.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace top::save_recovery {
namespace {
constexpr std::size_t kMaximumLine = 131072;
bool valid(const std::string& line);

std::uint32_t updateCrc(std::uint32_t crc, const unsigned char* bytes, std::size_t size) {
    for (std::size_t i = 0; i < size; ++i) {
        crc ^= bytes[i];
        for (int bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
    }
    return crc;
}

std::uint32_t checksum(const std::string& data) {
    return updateCrc(0xffffffffu,
        reinterpret_cast<const unsigned char*>(data.data()), data.size()) ^ 0xffffffffu;
}

std::string parentDirectory(const std::string& path) {
    const std::size_t slash = path.find_last_of('/');
    if (slash == std::string::npos) return ".";
    return slash == 0 ? "/" : path.substr(0, slash);
}

bool syncParent(const std::string& path) {
    const std::string parent = parentDirectory(path);
    const int fd = ::open(parent.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (fd < 0) return false;
    const bool okay = ::fsync(fd) == 0;
    ::close(fd);
    return okay;
}

bool readLine(const std::string& path, std::string& line) {
    const int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0) return false;
    line.clear();
    char bytes[4096];
    bool okay = true;
    bool finished = false;
    while (!finished) {
        const ssize_t count = ::read(fd, bytes, sizeof bytes);
        if (count < 0) {
            if (errno == EINTR) continue;
            okay = false;
            break;
        }
        if (count == 0) break;
        for (ssize_t i = 0; i < count; ++i) {
            line.push_back(bytes[i]);
            if (line.size() > kMaximumLine) {
                okay = false;
                finished = true;
                break;
            }
            if (bytes[i] == '\n') {
                finished = true;
                break;
            }
        }
    }
    ::close(fd);
    return okay && valid(line);
}

bool valid(const std::string& line) {
    int level = 0, exp = 0, gold = 0, hp = 0;
    int have0 = 0, have1 = 0, have2 = 0;
    int claimed0 = 0, claimed1 = 0, claimed2 = 0;
    const std::size_t trailer = line.rfind("|CRC1:");
    if (trailer != std::string::npos) {
        constexpr std::size_t kTrailerSize = 6 + 8 + 1;
        if (line.size() != trailer + kTrailerSize || line.back() != '\n') return false;
        std::uint32_t stored = 0;
        for (std::size_t i = trailer + 6; i < trailer + 14; ++i) {
            const unsigned char ch = static_cast<unsigned char>(line[i]);
            unsigned digit = 0;
            if (ch >= '0' && ch <= '9') digit = ch - '0';
            else if (ch >= 'A' && ch <= 'F') digit = ch - 'A' + 10;
            else if (ch >= 'a' && ch <= 'f') digit = ch - 'a' + 10;
            else return false;
            stored = (stored << 4) | digit;
        }
        if (checksum(line.substr(0, trailer)) != stored) return false;
    }
    return std::sscanf(line.c_str(), "%d %d %d %d %d %d %d %d %d %d",
                       &level, &exp, &gold, &hp, &have0, &have1, &have2,
                       &claimed0, &claimed1, &claimed2) == 10 &&
           level >= 1 && level <= 100 && exp >= 0 && gold >= 0 && hp >= 0 &&
           have0 >= 0 && have1 >= 0 && have2 >= 0 &&
           (claimed0 == 0 || claimed0 == 1) &&
           (claimed1 == 0 || claimed1 == 1) &&
           (claimed2 == 0 || claimed2 == 1);
}

bool copyFile(int from, int to) {
    char bytes[16384];
    for (;;) {
        const ssize_t count = ::read(from, bytes, sizeof bytes);
        if (count == 0) return true;
        if (count < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        ssize_t written = 0;
        while (written < count) {
            const ssize_t part = ::write(to, bytes + written,
                                        static_cast<std::size_t>(count - written));
            if (part < 0) {
                if (errno == EINTR) continue;
                return false;
            }
            written += part;
        }
    }
}
} // namespace

bool read(const std::string& path, std::string& line, bool& recovered) {
    recovered = false;
    if (readLine(path, line)) return true;
    if (!readLine(path + ".bak", line)) return false;
    recovered = true;
    return true;
}

bool preservePrevious(const std::string& path) {
    const int source = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
    if (source < 0) return errno == ENOENT;

    // Never rotate an invalid primary over a usable recovery copy. Legacy
    // saves without CRC1 still pass the bounded header validation above.
    std::string existingLine;
    if (!readLine(path, existingLine)) {
        ::close(source);
        return true;
    }

    const std::string backup = path + ".bak";
    std::string temporary = backup + ".tmp.XXXXXX";
    std::vector<char> name(temporary.begin(), temporary.end());
    name.push_back('\0');
    const int destination = ::mkstemp(name.data());
    if (destination < 0) {
        ::close(source);
        return false;
    }
    temporary.assign(name.data());
    const bool copied = copyFile(source, destination);
    ::close(source);
    const bool durable = copied && ::fsync(destination) == 0;
    const bool closed = ::close(destination) == 0;
    if (!durable || !closed || ::rename(temporary.c_str(), backup.c_str()) != 0) {
        ::unlink(temporary.c_str());
        return false;
    }
    if (!syncParent(backup)) return false;
    return true;
}

bool appendChecksum(FILE* file) {
    if (!file || std::fflush(file) != 0 || std::ferror(file)) return false;
    const int fd = ::fileno(file);
    struct stat status{};
    if (fd < 0 || ::fstat(fd, &status) != 0 || status.st_size < 0 ||
        static_cast<std::uint64_t>(status.st_size) > kMaximumLine) return false;
    std::uint32_t crc = 0xffffffffu;
    char bytes[8192];
    off_t offset = 0;
    while (offset < status.st_size) {
        const std::size_t wanted = static_cast<std::size_t>(
            std::min<off_t>(static_cast<off_t>(sizeof bytes), status.st_size - offset));
        const ssize_t count = ::pread(fd, bytes, wanted, offset);
        if (count < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        if (count == 0) return false;
        crc = updateCrc(crc, reinterpret_cast<const unsigned char*>(bytes),
                        static_cast<std::size_t>(count));
        offset += count;
    }
    crc ^= 0xffffffffu;
    if (::fseeko(file, 0, SEEK_END) != 0) return false;
    return std::fprintf(file, "|CRC1:%08X\n", crc) > 0 && !std::ferror(file);
}

} // namespace top::save_recovery
