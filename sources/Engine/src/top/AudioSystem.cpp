#include "top/AudioSystem.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#ifdef __EMSCRIPTEN__
#include <SDL.h>
#include <SDL_mixer.h>
#else
#include <dlfcn.h>
#endif
#include <vector>

namespace top {
namespace {

std::vector<std::string> tabs(const std::string& row) {
    std::vector<std::string> out;
    std::size_t at = 0;
    while (at <= row.size()) {
        const std::size_t end = row.find('\t', at);
        out.push_back(row.substr(at, end == std::string::npos ? std::string::npos : end - at));
        if (end == std::string::npos) break;
        at = end + 1;
    }
    return out;
}

std::string historicalRoot(const std::string& preferred) {
    for (const std::string& root : {preferred}) {
        FILE* file = std::fopen((root + "/scripts/table/musicinfo.txt").c_str(), "rb");
        if (!file) continue;
        std::fclose(file);
        return root;
    }
    return preferred;
}

int mapMusic(const std::string& map) {
    // Os cinco mapas-base seguem a numeracao inicial da tabela historica.
    if (map == "garner" || map == "garner2") return 1;
    if (map == "magicsea") return 2;
    if (map == "darkblue") return 3;
    if (map == "lonetower") return 4;
    if (map == "eastgoaf") return 5;
    return 1;
}

#ifndef __EMSCRIPTEN__
template <typename T>
bool symbol(void* library, const char* name, T& value) {
    value = reinterpret_cast<T>(dlsym(library, name));
    return value != nullptr;
}

#endif

}  // namespace

AudioSystem::AudioSystem() = default;
AudioSystem::~AudioSystem() { close(); }

bool AudioSystem::open(const std::string& clientRoot, const std::string& mapName) {
    close();
    root_ = historicalRoot(clientRoot);
    FILE* table = std::fopen((root_ + "/scripts/table/musicinfo.txt").c_str(), "rb");
    if (table) {
        char raw[2048];
        while (std::fgets(raw, sizeof raw, table)) {
            std::string row(raw);
            if (!row.empty() && row.back() == '\n') row.pop_back();
            if (!row.empty() && row.back() == '\r') row.pop_back();
            const std::vector<std::string> col = tabs(row);
            if (col.size() < 2) continue;
            const int id = std::atoi(col[0].c_str());
            if (id > 0 && !col[1].empty()) files_[id] = root_ + "/" + col[1];
        }
        std::fclose(table);
    }
    report_.tableLoaded = !files_.empty();
    report_.entries = static_cast<int>(files_.size());
    if (!report_.tableLoaded) return false;

    // AreaSet columns are: ID, name, color, nMusic, environment, light,
    // direction, city. WorldScene switches background music when the tile's
    // island/area changes, not merely when the map changes.
    FILE* areas = std::fopen((root_ + "/scripts/table/areaset.txt").c_str(), "rb");
    if (areas) {
        char raw[2048];
        while (std::fgets(raw, sizeof raw, areas)) {
            std::string row(raw);
            if (!row.empty() && row.back() == '\n') row.pop_back();
            if (!row.empty() && row.back() == '\r') row.pop_back();
            if (row.empty() || row[0] == '/' || row[0] == '#') continue;
            const std::vector<std::string> col = tabs(row);
            if (col.size() < 4) continue;
            const int areaId = std::atoi(col[0].c_str());
            const int musicId = std::atoi(col[3].c_str());
            if (areaId > 0 && areaId <= 255 && musicId > 0 && files_.count(musicId))
                areaMusic_[areaId] = musicId;
        }
        std::fclose(areas);
    }

#ifdef __EMSCRIPTEN__
    sdlInitSubSystem_ = SDL_InitSubSystem;
    sdlQuitSubSystem_ = SDL_QuitSubSystem;
    sdlRwFromFile_ = [](const char* path, const char* mode) -> void* { return SDL_RWFromFile(path, mode); };
    mixOpenAudio_ = Mix_OpenAudio;
    mixCloseAudio_ = Mix_CloseAudio;
    mixLoadWavRw_ = [](void* rw, int close) -> void* { return Mix_LoadWAV_RW(static_cast<SDL_RWops*>(rw), close); };
    mixLoadMus_ = [](const char* path) -> void* { return Mix_LoadMUS(path); };
    mixPlayChannelTimed_ = [](int channel, void* chunk, int loops, int ticks) {
        return Mix_PlayChannelTimed(channel, static_cast<Mix_Chunk*>(chunk), loops, ticks);
    };
    mixPlayMusic_ = [](void* music, int loops) { return Mix_PlayMusic(static_cast<Mix_Music*>(music), loops); };
    mixFreeChunk_ = [](void* chunk) { Mix_FreeChunk(static_cast<Mix_Chunk*>(chunk)); };
    mixFreeMusic_ = [](void* music) { Mix_FreeMusic(static_cast<Mix_Music*>(music)); };
    mixHaltMusic_ = Mix_HaltMusic;
    mixVolumeMusic_ = Mix_VolumeMusic;
    mixVolume_ = Mix_Volume;
    Mix_Init(MIX_INIT_OGG);
#else
    sdl_ = dlopen("libSDL2-2.0.so.0", RTLD_NOW | RTLD_LOCAL);
    mixer_ = dlopen("libSDL2_mixer-2.0.so.0", RTLD_NOW | RTLD_LOCAL);
    if (!sdl_ || !mixer_ || !symbol(sdl_, "SDL_InitSubSystem", sdlInitSubSystem_) ||
        !symbol(sdl_, "SDL_QuitSubSystem", sdlQuitSubSystem_) || !symbol(sdl_, "SDL_RWFromFile", sdlRwFromFile_) ||
        !symbol(mixer_, "Mix_OpenAudio", mixOpenAudio_) || !symbol(mixer_, "Mix_CloseAudio", mixCloseAudio_) ||
        !symbol(mixer_, "Mix_LoadWAV_RW", mixLoadWavRw_) || !symbol(mixer_, "Mix_LoadMUS", mixLoadMus_) ||
        !symbol(mixer_, "Mix_PlayChannelTimed", mixPlayChannelTimed_) ||
        !symbol(mixer_, "Mix_PlayMusic", mixPlayMusic_) || !symbol(mixer_, "Mix_FreeChunk", mixFreeChunk_) ||
        !symbol(mixer_, "Mix_FreeMusic", mixFreeMusic_) || !symbol(mixer_, "Mix_HaltMusic", mixHaltMusic_))
        return true;
    symbol(mixer_, "Mix_VolumeMusic", mixVolumeMusic_);
    symbol(mixer_, "Mix_Volume", mixVolume_);
#endif
    constexpr unsigned kSdlAudio = 0x10u;
    constexpr unsigned short kAudioS16Sys = 0x8010u;
    if (sdlInitSubSystem_(kSdlAudio) != 0 || mixOpenAudio_(44100, kAudioS16Sys, 2, 1024) != 0) return true;
    report_.deviceOpened = true;
    setVolumes(musicVolume_, effectsVolume_);
    setMap(mapName);
    return true;
}

void AudioSystem::setVolumes(int music, int effects) {
    musicVolume_ = std::max(0, std::min(128, music));
    effectsVolume_ = std::max(0, std::min(128, effects));
    if (mixVolumeMusic_) mixVolumeMusic_(musicVolume_);
    if (mixVolume_) mixVolume_(-1, effectsVolume_);
}

void AudioSystem::close() {
    if (report_.deviceOpened) {
        if (mixHaltMusic_) mixHaltMusic_();
        if (music_ && mixFreeMusic_) mixFreeMusic_(music_);
        for (auto& chunk : chunks_) if (chunk.second && mixFreeChunk_) mixFreeChunk_(chunk.second);
        if (mixCloseAudio_) mixCloseAudio_();
        if (sdlQuitSubSystem_) sdlQuitSubSystem_(0x10u);
    }
    music_ = nullptr;
    chunks_.clear();
    files_.clear();
    areaMusic_.clear();
#ifdef __EMSCRIPTEN__
    Mix_Quit();
#else
    if (mixer_) dlclose(mixer_);
    if (sdl_) dlclose(sdl_);
#endif
    mixer_ = nullptr;
    sdl_ = nullptr;
    report_ = AudioReport{};
}

void AudioSystem::setMap(const std::string& mapName) {
    if (!report_.deviceOpened) return;
    playMusic(mapMusic(mapName));
}

void AudioSystem::setArea(int areaId) {
    if (!report_.deviceOpened) return;
    const auto music = areaMusic_.find(areaId);
    if (music != areaMusic_.end()) playMusic(music->second);
}

void AudioSystem::playMusic(int musicId) {
    const auto file = files_.find(musicId);
    if (file == files_.end() || report_.music == file->second) return;
    if (mixHaltMusic_) mixHaltMusic_();
    if (music_ && mixFreeMusic_) mixFreeMusic_(music_);
    music_ = mixLoadMus_(file->second.c_str());
    report_.music = file->second;
    if (music_) mixPlayMusic_(music_, -1);
}

void AudioSystem::emit(const EffectEvent& event) {
    if (!report_.deviceOpened) return;
    // IDs 40 em diante sao os WAV de combate em musicinfo.txt.
    const int id = event.variant >= 900 ? 48 :
                   (event.kind == EffectEvent::Cast ? 40 : event.kind == EffectEvent::SkillHit ? 47 :
                    event.kind == EffectEvent::Hurt ? 43 : 41);
    Chunk*& chunk = chunks_[id];
    if (!chunk) {
        const auto file = files_.find(id);
        if (file == files_.end()) return;
        void* rw = sdlRwFromFile_(file->second.c_str(), "rb");
        if (!rw) return;
        chunk = mixLoadWavRw_(rw, 1);
    }
    if (chunk) mixPlayChannelTimed_(-1, chunk, 0, -1);
}

void AudioSystem::emitFootstep(int surface) {
    if (!report_.deviceOpened) return;
    // musicinfo.txt reserva 21..33 para os passos de personagem. A tabela
    // usa variantes por superfície; sem uma classificação completa do mapa,
    // mantém-se a seleção determinística e offline por superfície/variante.
    const int id = 21 + (std::max(0, surface) % 13);
    Chunk*& chunk = chunks_[id];
    if (!chunk) {
        const auto file = files_.find(id);
        if (file == files_.end()) return;
        void* rw = sdlRwFromFile_(file->second.c_str(), "rb");
        if (!rw) return;
        chunk = mixLoadWavRw_(rw, 1);
    }
    if (chunk) mixPlayChannelTimed_(-1, chunk, 0, -1);
}

bool AudioSystem::selfTest(const std::string& clientRoot) {
    const std::string root = historicalRoot(clientRoot);
    FILE* table = std::fopen((root + "/scripts/table/musicinfo.txt").c_str(), "rb");
    if (!table) return false;
    int rows = 0;
    char raw[2048];
    while (std::fgets(raw, sizeof raw, table)) {
        const std::vector<std::string> col = tabs(raw);
        if (col.size() >= 2 && std::atoi(col[0].c_str()) > 0) ++rows;
    }
    std::fclose(table);
    FILE* areaTable = std::fopen((root + "/scripts/table/areaset.txt").c_str(), "rb");
    if (!areaTable) return false;
    bool argentTrack = false, thundoriaTrack = false, shaitanTrack = false;
    char areaRow[2048];
    while (std::fgets(areaRow, sizeof areaRow, areaTable)) {
        const std::vector<std::string> col = tabs(areaRow);
        if (col.size() < 4) continue;
        const int id = std::atoi(col[0].c_str());
        const int musicId = std::atoi(col[3].c_str());
        argentTrack = argentTrack || (id == 1 && musicId == 2);
        thundoriaTrack = thundoriaTrack || (id == 8 && musicId == 3);
        shaitanTrack = shaitanTrack || (id == 13 && musicId == 4);
    }
    std::fclose(areaTable);
    FILE* music = std::fopen((root + "/music/01.ogg").c_str(), "rb");
    FILE* hit = std::fopen((root + "/music/sound/combat/01.wav").c_str(), "rb");
    unsigned char ogg[4] = {};
    unsigned char wav[4] = {};
    if (music) std::fread(ogg, 1, sizeof ogg, music);
    if (hit) std::fread(wav, 1, sizeof wav, hit);
#ifdef __EMSCRIPTEN__
    const bool runtime = true; // SDL_mixer is linked into WASM.
#else
    void* sdl = dlopen("libSDL2-2.0.so.0", RTLD_NOW | RTLD_LOCAL);
    void* mixer = dlopen("libSDL2_mixer-2.0.so.0", RTLD_NOW | RTLD_LOCAL);
    const bool runtime = sdl && mixer && dlsym(sdl, "SDL_RWFromFile") && dlsym(mixer, "Mix_LoadMUS") &&
                         dlsym(mixer, "Mix_LoadWAV_RW") && dlsym(mixer, "Mix_PlayMusic") &&
                         dlsym(mixer, "Mix_PlayChannelTimed");
#endif
    const bool okay = rows >= 100 && music && hit &&
                      std::equal(ogg, ogg + 4, reinterpret_cast<const unsigned char*>("OggS")) &&
                      std::equal(wav, wav + 4, reinterpret_cast<const unsigned char*>("RIFF")) && runtime;
    if (music) std::fclose(music);
    if (hit) std::fclose(hit);
#ifndef __EMSCRIPTEN__
    if (mixer) dlclose(mixer);
    if (sdl) dlclose(sdl);
#endif
    return okay && argentTrack && thundoriaTrack && shaitanTrack && mapMusic("darkblue") == 3;
}

}  // namespace top
