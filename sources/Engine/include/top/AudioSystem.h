#pragma once

// Audio offline do cliente historico. SDL2/SDL2_mixer sao abertos em runtime,
// portanto a compilacao nao depende dos headers nem de um servidor de jogo.

#include "top/EffectSystem.h"

#include <string>
#include <unordered_map>

namespace top {

struct AudioReport {
    bool tableLoaded = false;
    bool deviceOpened = false;
    int entries = 0;
    std::string music;
};

class AudioSystem {
public:
    AudioSystem();
    ~AudioSystem();
    bool open(const std::string& clientRoot, const std::string& mapName);
    void close();
    void setMap(const std::string& mapName);
    void setArea(int areaId);
    void setVolumes(int music, int effects);
    int musicVolume() const { return musicVolume_; }
    int effectsVolume() const { return effectsVolume_; }
    void emit(const EffectEvent& event);
    void emitFootstep(int surface = 0);
    static bool selfTest(const std::string& clientRoot);
    const AudioReport& report() const { return report_; }

private:
    using Chunk = void;
    using Music = void;
    void* sdl_ = nullptr;
    void* mixer_ = nullptr;
    std::string root_;
    std::unordered_map<int, std::string> files_;
    std::unordered_map<int, int> areaMusic_;
    std::unordered_map<int, Chunk*> chunks_;
    Music* music_ = nullptr;
    AudioReport report_;

    int (*sdlInitSubSystem_)(unsigned) = nullptr;
    void (*sdlQuitSubSystem_)(unsigned) = nullptr;
    void* (*sdlRwFromFile_)(const char*, const char*) = nullptr;
    int (*mixOpenAudio_)(int, unsigned short, int, int) = nullptr;
    void (*mixCloseAudio_)() = nullptr;
    Chunk* (*mixLoadWavRw_)(void*, int) = nullptr;
    Music* (*mixLoadMus_)(const char*) = nullptr;
    int (*mixPlayChannelTimed_)(int, Chunk*, int, int) = nullptr;
    int (*mixPlayMusic_)(Music*, int) = nullptr;
    void (*mixFreeChunk_)(Chunk*) = nullptr;
    void (*mixFreeMusic_)(Music*) = nullptr;
    int (*mixHaltMusic_)() = nullptr;
    int (*mixVolumeMusic_)(int) = nullptr;
    int (*mixVolume_)(int, int) = nullptr;
    int musicVolume_ = 96;
    int effectsVolume_ = 96;

    void playMusic(int musicId);
};

}  // namespace top
