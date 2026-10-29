#include "top/Cursor.h"

#include "top/Image.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>
#include <unordered_map>
#include <vector>

namespace top {
namespace {

bool readU16(const std::uint8_t* p, std::size_t n, std::size_t off, std::uint16_t& out) {
    if (off + 2 > n) return false;
    out = static_cast<std::uint16_t>(p[off] | (p[off + 1] << 8));
    return true;
}

bool readU32(const std::uint8_t* p, std::size_t n, std::size_t off, std::uint32_t& out) {
    if (off + 4 > n) return false;
    out = static_cast<std::uint32_t>(p[off] | (p[off + 1] << 8) | (p[off + 2] << 16) | (p[off + 3] << 24));
    return true;
}

bool readI32(const std::uint8_t* p, std::size_t n, std::size_t off, std::int32_t& out) {
    std::uint32_t u = 0;
    if (!readU32(p, n, off, u)) return false;
    out = static_cast<std::int32_t>(u);
    return true;
}

bool readAll(const std::string& path, std::vector<std::uint8_t>& out) {
    FILE* file = std::fopen(path.c_str(), "rb");
    if (!file) return false;
    if (std::fseek(file, 0, SEEK_END) != 0) {
        std::fclose(file);
        return false;
    }
    const long length = std::ftell(file);
    if (length <= 0 || length > 2 * 1024 * 1024) {
        std::fclose(file);
        return false;
    }
    std::fseek(file, 0, SEEK_SET);
    out.resize(static_cast<std::size_t>(length));
    const bool ok = std::fread(out.data(), 1, out.size(), file) == out.size();
    std::fclose(file);
    return ok;
}

bool decodeDib(const std::uint8_t* dib, std::size_t n, int entryW, int entryH, int hotX, int hotY, CursorFrame& out) {
    std::int32_t biSize = 0;
    std::int32_t bw = 0;
    std::int32_t bh = 0;
    std::uint16_t bit = 0;
    std::uint32_t clrUsed = 0;
    if (!readI32(dib, n, 0, biSize) || !readI32(dib, n, 4, bw) || !readI32(dib, n, 8, bh) || !readU16(dib, n, 14, bit) ||
        !readU32(dib, n, 32, clrUsed))
        return false;
    if (biSize < 40 || biSize > 256 || static_cast<std::size_t>(biSize) > n) return false;
    const int width = entryW > 0 ? entryW : bw;
    const int height = entryH > 0 ? entryH : (bh < 0 ? -bh : bh) / 2;
    if (width <= 0 || height <= 0 || width > 256 || height > 256) return false;
    const bool topDown = bh < 0;
    int palN = 0;
    if (bit <= 8) palN = clrUsed != 0 ? static_cast<int>(clrUsed) : (1 << bit);
    if (palN < 0 || palN > 256) return false;
    const std::size_t palOff = static_cast<std::size_t>(biSize);
    if (palOff + static_cast<std::size_t>(palN) * 4 > n) return false;
    const int xorStride = ((width * static_cast<int>(bit) + 31) / 32) * 4;
    const int andStride = ((width + 31) / 32) * 4;
    const std::size_t xorOff = palOff + static_cast<std::size_t>(palN) * 4;
    const std::size_t andOff = xorOff + static_cast<std::size_t>(xorStride) * static_cast<std::size_t>(height);
    if (xorStride < 0 || andOff + static_cast<std::size_t>(andStride) * static_cast<std::size_t>(height) > n) return false;

    out.w = width;
    out.h = height;
    out.hotX = hotX;
    out.hotY = hotY;
    out.opaque = 0;
    out.rgba.assign(static_cast<std::size_t>(width * height * 4), 0);
    for (int y = 0; y < height; ++y) {
        const int sy = topDown ? y : (height - 1 - y);
        const std::size_t row = xorOff + static_cast<std::size_t>(sy * xorStride);
        const std::size_t arow = andOff + static_cast<std::size_t>(sy * andStride);
        for (int x = 0; x < width; ++x) {
            int r = 0;
            int g = 0;
            int b = 0;
            int a = 255;
            if (bit == 32) {
                b = dib[row + static_cast<std::size_t>(x) * 4];
                g = dib[row + static_cast<std::size_t>(x) * 4 + 1];
                r = dib[row + static_cast<std::size_t>(x) * 4 + 2];
                a = dib[row + static_cast<std::size_t>(x) * 4 + 3];
            } else if (bit == 24) {
                b = dib[row + static_cast<std::size_t>(x) * 3];
                g = dib[row + static_cast<std::size_t>(x) * 3 + 1];
                r = dib[row + static_cast<std::size_t>(x) * 3 + 2];
            } else if (bit == 8) {
                const int idx = dib[row + static_cast<std::size_t>(x)];
                if (idx < 0 || idx >= palN) return false;
                b = dib[palOff + static_cast<std::size_t>(idx) * 4];
                g = dib[palOff + static_cast<std::size_t>(idx) * 4 + 1];
                r = dib[palOff + static_cast<std::size_t>(idx) * 4 + 2];
            } else if (bit == 4) {
                const int byte = dib[row + static_cast<std::size_t>(x / 2)];
                const int idx = (x % 2 == 0) ? (byte >> 4) : (byte & 0x0f);
                if (idx >= palN) return false;
                b = dib[palOff + static_cast<std::size_t>(idx) * 4];
                g = dib[palOff + static_cast<std::size_t>(idx) * 4 + 1];
                r = dib[palOff + static_cast<std::size_t>(idx) * 4 + 2];
            } else if (bit == 1) {
                const int byte = dib[row + static_cast<std::size_t>(x / 8)];
                const int idx = (byte >> (7 - (x % 8))) & 1;
                if (palN > idx) {
                    b = dib[palOff + static_cast<std::size_t>(idx) * 4];
                    g = dib[palOff + static_cast<std::size_t>(idx) * 4 + 1];
                    r = dib[palOff + static_cast<std::size_t>(idx) * 4 + 2];
                } else {
                    r = g = b = idx ? 255 : 0;
                }
            } else {
                return false;
            }
            const int mask = (dib[arow + static_cast<std::size_t>(x / 8)] >> (7 - (x % 8))) & 1;
            // AND 1 com preto e o furo. AND 0 e a cor. AND 1 com cor e o XOR da tela:
            // a cor fica, para o contorno nao sumir.
            if (mask == 1 && r < 8 && g < 8 && b < 8) a = 0;
            else if (bit == 32 && a < 16 && mask == 1) a = 0;
            else if (bit == 32 && mask == 0) a = 255;
            if (a != 0) ++out.opaque;
            const std::size_t o = static_cast<std::size_t>((y * width + x) * 4);
            out.rgba[o] = static_cast<std::uint8_t>(r);
            out.rgba[o + 1] = static_cast<std::uint8_t>(g);
            out.rgba[o + 2] = static_cast<std::uint8_t>(b);
            out.rgba[o + 3] = static_cast<std::uint8_t>(a);
        }
    }
    return true;
}

bool decodeIconBlob(const std::uint8_t* blob, std::size_t n, std::vector<CursorFrame>& into) {
    std::uint16_t type = 0;
    std::uint16_t count = 0;
    if (!readU16(blob, n, 2, type) || !readU16(blob, n, 4, count)) return false;
    if ((type != 1 && type != 2) || count == 0 || count > 64) return false;
    bool any = false;
    for (std::uint16_t i = 0; i < count; ++i) {
        const std::size_t ent = 6 + static_cast<std::size_t>(i) * 16;
        if (ent + 16 > n) return false;
        int w = blob[ent];
        int h = blob[ent + 1];
        if (w == 0) w = 256;
        if (h == 0) h = 256;
        std::uint16_t a = 0;
        std::uint16_t b = 0;
        std::uint32_t bytes = 0;
        std::uint32_t offset = 0;
        if (!readU16(blob, n, ent + 4, a) || !readU16(blob, n, ent + 6, b) || !readU32(blob, n, ent + 8, bytes) ||
            !readU32(blob, n, ent + 12, offset))
            return false;
        const int hotX = type == 2 ? static_cast<int>(a) : 0;
        const int hotY = type == 2 ? static_cast<int>(b) : 0;
        if (offset >= n || bytes < 40 || offset + bytes > n) return false;
        CursorFrame frame;
        if (!decodeDib(blob + offset, bytes, w, h, hotX, hotY, frame)) return false;
        into.push_back(std::move(frame));
        any = true;
    }
    return any;
}

int countFiles(const std::string& path) {
    DIR* dir = opendir(path.c_str());
    if (!dir) return 0;
    int n = 0;
    while (const dirent* ent = readdir(dir)) {
        if (ent->d_name[0] == '.' &&
            (ent->d_name[1] == '\0' || (ent->d_name[1] == '.' && ent->d_name[2] == '\0')))
            continue;
        const std::string child = path + "/" + ent->d_name;
        struct stat st;
        if (stat(child.c_str(), &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) n += countFiles(child);
        else if (S_ISREG(st.st_mode)) ++n;
    }
    closedir(dir);
    return n;
}

}  // namespace

bool GameCursor::decodeFile(const std::string& path, Clip& clip) {
    clip = Clip();
    std::vector<std::uint8_t> data;
    if (!readAll(path, data)) return false;
    std::vector<CursorFrame> got;
    std::uint32_t declared = 0;
    std::uint32_t steps = 0;
    std::uint32_t jif = 10;
    std::vector<int> seq;
    std::vector<std::uint32_t> rate;
    const std::uint8_t* p = data.data();
    const std::size_t n = data.size();
    if (n >= 12 && std::memcmp(p, "RIFF", 4) == 0 && std::memcmp(p + 8, "ACON", 4) == 0) {
        std::size_t off = 12;
        while (off + 8 <= n) {
            char id[5] = {};
            std::memcpy(id, p + off, 4);
            std::uint32_t sz = 0;
            if (!readU32(p, n, off + 4, sz)) return false;
            if (off + 8 + sz > n) return false;
            const std::uint8_t* body = p + off + 8;
            if (std::memcmp(id, "anih", 4) == 0 && sz >= 36) {
                if (!readU32(body, sz, 4, declared) || !readU32(body, sz, 8, steps) || !readU32(body, sz, 28, jif))
                    return false;
            } else if (std::memcmp(id, "seq ", 4) == 0) {
                for (std::uint32_t i = 0; i + 4 <= sz; i += 4) {
                    std::uint32_t v = 0;
                    if (!readU32(body, sz, i, v)) return false;
                    seq.push_back(static_cast<int>(v));
                }
            } else if (std::memcmp(id, "rate", 4) == 0) {
                for (std::uint32_t i = 0; i + 4 <= sz; i += 4) {
                    std::uint32_t v = 0;
                    if (!readU32(body, sz, i, v)) return false;
                    rate.push_back(v);
                }
            } else if (std::memcmp(id, "LIST", 4) == 0 && sz >= 4 && std::memcmp(body, "fram", 4) == 0) {
                std::size_t inner = 4;
                while (inner + 8 <= sz) {
                    std::uint32_t isz = 0;
                    if (!readU32(body, sz, inner + 4, isz)) return false;
                    if (inner + 8 + isz > sz) return false;
                    if (std::memcmp(body + inner, "icon", 4) == 0) {
                        if (!decodeIconBlob(body + inner + 8, isz, got)) return false;
                    }
                    inner += 8 + isz + (isz & 1u);
                }
            }
            off += 8 + sz + (sz & 1u);
        }
    } else if (!decodeIconBlob(p, n, got)) {
        return false;
    }
    if (got.empty()) return false;
    if (steps == 0) steps = static_cast<std::uint32_t>(got.size());
    if (jif == 0) jif = 10;
    if (seq.size() != steps) {
        seq.clear();
        for (std::uint32_t i = 0; i < steps; ++i) seq.push_back(static_cast<int>(i % got.size()));
    }
    for (int& index : seq) {
        if (index < 0 || index >= static_cast<int>(got.size())) index = 0;
    }
    clip.first = static_cast<int>(frames_.size());
    clip.count = static_cast<int>(got.size());
    clip.seq = std::move(seq);
    clip.hold.assign(steps, 0.f);
    for (std::uint32_t i = 0; i < steps; ++i) {
        const std::uint32_t tick = (i < rate.size() && rate[i] != 0) ? rate[i] : jif;
        clip.hold[i] = static_cast<float>(tick) / 60.f;
    }
    for (CursorFrame& frame : got) frames_.push_back(std::move(frame));
    clip.ok = clip.count > 0 && !clip.seq.empty();
    return clip.ok;
}

bool GameCursor::load(const std::string& clientRoot) {
    frames_.clear();
    filesOk_ = 0;
    state_ = CurNormal;
    step_ = 0;
    time_ = 0.f;
    for (Clip& clip : clips_) clip = Clip();
    struct Bind {
        int state;
        const char* file;
    };
    const Bind binds[] = {
        {CurNormal, "normal.ani"}, {CurActive, "mouseon.ani"}, {CurDrag, "drag.ani"},
        {CurSize, "drag.ani"},     {CurVertical, "vertical.ani"}, {CurLevel, "drag.ani"},
        {CurWait, "wait.ani"},     {CurAttack, "attack.ani"},  {CurSkill, "skill.ani"},
        {CurLand, "land.ani"},     {CurSail, "sail.ani"},      {CurForbid, "forbid.ani"},
        {CurHover, "mouseon.ani"}, {CurPick, "pick.ani"},      {CurCamera, "camera.ani"},
        {CurChat, "chat.ani"},     {CurRight, "mouseright.ani"}, {CurClick, "mouseclick.ani"},
        {CurHide, "hide.ani"},     {CurSearch, "search.ani"},  {CurBlock, "block.ani"},
        {CurRepair, "repair.ani"}, {CurFeed, "feed.ani"},
    };
    std::unordered_map<std::string, Clip> decoded;
    for (const Bind& bind : binds) {
        const std::string name = bind.file;
        if (!decoded.count(name)) {
            Clip clip;
            if (decodeFile(clientRoot + "/cursor/" + name, clip)) ++filesOk_;
            decoded.emplace(name, std::move(clip));
        }
        clips_[bind.state] = decoded[name];
    }
    return filesOk_ == 20 && clips_[CurNormal].ok;
}

void GameCursor::setState(int state) {
    if (state < 0 || state >= CurCount) state = CurNormal;
    if (state == state_) return;
    state_ = state;
    time_ = 0.f;
    step_ = 0;
}

void GameCursor::advance(float dt) {
    if (dt < 0.f) dt = 0.f;
    const Clip& clip = clips_[state_];
    if (!clip.ok || clip.hold.empty()) {
        step_ = 0;
        return;
    }
    time_ += dt;
    float total = 0.f;
    for (float hold : clip.hold) total += hold;
    if (total < 1e-4f) {
        step_ = 0;
        return;
    }
    float t = time_;
    while (t >= total) t -= total;
    step_ = 0;
    for (int i = 0; i < static_cast<int>(clip.hold.size()); ++i) {
        if (t < clip.hold[static_cast<std::size_t>(i)]) {
            step_ = i;
            return;
        }
        t -= clip.hold[static_cast<std::size_t>(i)];
    }
    step_ = static_cast<int>(clip.hold.size()) - 1;
}

int GameCursor::framesIn(int state) const {
    if (state < 0 || state >= CurCount) return 0;
    return clips_[state].count;
}

const CursorFrame* GameCursor::frame() const {
    const int slot = frameSlot();
    if (slot < 0 || slot >= static_cast<int>(frames_.size())) return nullptr;
    return &frames_[static_cast<std::size_t>(slot)];
}

int GameCursor::frameSlot() const {
    const Clip& clip = clips_[state_];
    if (!clip.ok || clip.seq.empty()) return -1;
    int step = step_;
    if (step < 0 || step >= static_cast<int>(clip.seq.size())) step = 0;
    const int local = clip.seq[static_cast<std::size_t>(step)];
    if (local < 0 || local >= clip.count) return -1;
    return clip.first + local;
}

bool GameCursor::selfTest(const std::string& root) {
    GameCursor cursor;
    if (!cursor.load(root) || cursor.fileCount() != 20) {
        std::fprintf(stderr, "cursor: abri %d de 20\n", cursor.fileCount());
        return false;
    }
    if (cursor.frames().size() != 66) {
        std::fprintf(stderr, "cursor: %zu quadros\n", cursor.frames().size());
        return false;
    }
    if (cursor.framesIn(CurNormal) != 4 || cursor.framesIn(CurAttack) != 2 || cursor.framesIn(CurWait) != 18 ||
        cursor.framesIn(CurSkill) != 3 || cursor.framesIn(CurBlock) != 1 || cursor.framesIn(CurHide) != 1 ||
        cursor.framesIn(CurSearch) != 8 || cursor.framesIn(CurChat) != 6) {
        std::fprintf(stderr, "cursor: contagem de quadros diferente do .ani\n");
        return false;
    }
    cursor.setState(CurNormal);
    cursor.advance(0.f);
    const CursorFrame* arrow = cursor.frame();
    if (!arrow || arrow->w != 32 || arrow->h != 32 || arrow->hotX != 0 || arrow->hotY != 0 || arrow->opaque < 100) {
        std::fprintf(stderr, "cursor: normal sem a seta\n");
        return false;
    }
    const std::uint8_t* px = arrow->rgba.data() + (5 * 32 + 3) * 4;
    if (px[0] < 180 || px[1] < 180 || px[2] < 180 || px[3] != 255) {
        std::fprintf(stderr, "cursor: pixel da seta %u %u %u %u\n", px[0], px[1], px[2], px[3]);
        return false;
    }
    cursor.advance(17.f / 60.f + 0.001f);
    if (cursor.stepIndex() != 1 || cursor.frame() == arrow) {
        std::fprintf(stderr, "cursor: normal nao avancou o quadro\n");
        return false;
    }
    cursor.setState(CurClick);
    const CursorFrame* click = cursor.frame();
    if (!click || click->hotX != 1 || click->hotY != 1) {
        std::fprintf(stderr, "cursor: mouseclick sem o ponto quente\n");
        return false;
    }
    cursor.setState(CurWait);
    const CursorFrame* wait = cursor.frame();
    if (!wait || wait->hotX != 6 || wait->hotY != 6 || wait->opaque < 100) {
        std::fprintf(stderr, "cursor: espera sem quadro\n");
        return false;
    }
    cursor.setState(CurHide);
    const CursorFrame* hide = cursor.frame();
    if (!hide || hide->w != 32 || hide->opaque != 0) {
        std::fprintf(stderr, "cursor: hide devia ser vazio\n");
        return false;
    }
    cursor.setState(CurBlock);
    const CursorFrame* block = cursor.frame();
    if (!block || block->opaque < 400) {
        std::fprintf(stderr, "cursor: block sem o sinal\n");
        return false;
    }
    cursor.setState(CurSkill);
    cursor.advance(0.f);
    const CursorFrame* skill0 = cursor.frame();
    cursor.advance(10.f / 60.f + 0.001f);
    const CursorFrame* skill1 = cursor.frame();
    cursor.advance(10.f / 60.f + 0.001f);
    const CursorFrame* skill2 = cursor.frame();
    cursor.advance(10.f / 60.f + 0.001f);
    const CursorFrame* skill3 = cursor.frame();
    if (!skill0 || !skill1 || !skill2 || skill0 == skill1 || skill1 == skill2 || skill3 != skill1 ||
        cursor.stepIndex() != 3) {
        std::fprintf(stderr, "cursor: skill nao girou 0,1,2,1\n");
        return false;
    }

    const int icon = countFiles(root + "/texture/icon");
    const int ui = countFiles(root + "/texture/ui");
    const int effect = countFiles(root + "/texture/effect");
    const int item = countFiles(root + "/texture/item");
    const int photo = countFiles(root + "/texture/photo");
    const int minimap = countFiles(root + "/texture/minimap");
    const int bigmap = countFiles(root + "/texture/bigmap");
    const int lit = countFiles(root + "/texture/lit");
    const int skillTex = countFiles(root + "/texture/skill");
    Image image;
    const bool iconOk = image.load(root + "/texture/icon/0021.png") && image.width > 0;
    const int iconW = image.width;
    const bool effectOk = image.load(root + "/texture/effect/00000008.tga") && image.width > 0;
    const bool itemOk = image.load(root + "/texture/item/0000114002.bmp") && image.width > 0;
    const bool logoOk = image.load(root + "/texture/logo.bmp") && image.width > 0;
    std::printf("cursor %d arquivos, %zu quadros, normal %d opacos, icon %d px\n", cursor.fileCount(),
                cursor.frames().size(), arrow->opaque, iconW);
    std::printf("texturas icon %d, ui %d, efeito %d, item %d, foto %d, minimapa %d, bigmap %d, lit %d, skill %d\n",
                icon, ui, effect, item, photo, minimap, bigmap, lit, skillTex);
    std::fflush(stdout);
    if (!iconOk || !effectOk || !itemOk || !logoOk) {
        std::fprintf(stderr, "textura de volta falhou icon %d efeito %d item %d logo %d\n", iconOk ? 1 : 0,
                     effectOk ? 1 : 0, itemOk ? 1 : 0, logoOk ? 1 : 0);
        return false;
    }
    if (icon < 4000 || ui < 1500 || effect < 1100 || item < 700 || photo < 700 || minimap < 80 || bigmap < 10 ||
        lit < 20 || skillTex < 1) {
        std::fprintf(stderr, "texturas incompletas\n");
        return false;
    }
    return true;
}

}  // namespace top
