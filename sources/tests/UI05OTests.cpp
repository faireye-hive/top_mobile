#include "top/CharacterPoseBinding.h"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

static std::vector<std::string> splitTab(const std::string& line) {
    std::vector<std::string> parts;
    std::size_t at=0;
    while (at<=line.size()) {
        const auto sep=line.find('\t',at);
        parts.push_back(line.substr(at,sep==std::string::npos?std::string::npos:sep-at));
        if (sep==std::string::npos) break;
        at=sep+1;
    }
    return parts;
}

int main() {
    auto row=[](const std::string& key){ std::vector<std::string> c(21);c[20]=key;return c; };
    // These are confirmed in the original characterinfo.txt.
    assert(top::characterPoseSetId(103,row("114"))==114); // Forest Spirit
    assert(top::characterPoseSetId(119,row("84"))==84);   // Grass Tortoise
    assert(top::characterPoseSetId(185,row("75"))==75);   // Mystic Flower
    assert(top::characterPoseSetId(127,row("71"))==71);   // Slowpoke Snail
    assert(top::characterPoseSetId(6,row("71"))==71);     // Weird Monster
    assert(top::characterPoseSetId(1,row("1"))==1);       // playable character
    assert(top::characterPoseSetId(42,row("0"))==42);
    assert(top::characterPoseSetId(42,row(""))==42);
    assert(top::characterPoseSetId(42,row("broken"))==42);
    assert(top::characterPoseSetId(42,row("-100"))==42);
    assert(top::characterPoseSetId(42,row("9999999999"))==42);
    assert(top::characterPoseSetId(42,row("71,0"))==71);
    assert(top::characterPoseSetId(42,row("71foo"))==42);
    assert(top::characterPoseSetId(42,{})==42);
    assert(top::poseFrameOnce(12,32,-5.f,40)==12);
    assert(top::poseFrameOnce(12,32,8.9f,40)==20);
    assert(top::poseFrameOnce(12,32,1000.f,40)==32);
    assert(top::poseFrameOnce(12,80,1000.f,40)==39);
    assert(top::poseFrameOnce(12,32,5.f,0)==0);

    // Optional on-disk regression using the original scripts: tests remain
    // self-contained when the Client assets have not been copied to CI.
    std::ifstream characters("../Client/scripts/table/characterinfo.txt");
    std::ifstream actions("../Client/scripts/txt/CharacterAction.tx");
    if (!characters.is_open() || !actions.is_open()) {
        characters.open("Client/scripts/table/characterinfo.txt");
        actions.open("Client/scripts/txt/CharacterAction.tx");
    }
    if (characters.is_open() && actions.is_open()) {
        std::unordered_map<int,int> actionOf;
        for (std::string line; std::getline(characters,line);) {
            if (line.empty() || line[0]=='/') continue;
            auto cols=splitTab(line);
            if (cols.size()<=20) continue;
            const int id=std::atoi(cols[0].c_str());
            if (id>0) actionOf[id]=top::characterPoseSetId(id,cols);
        }
        std::unordered_set<int> banks;
        for (std::string line; std::getline(actions,line);) {
            if (line.empty() || line[0]=='/' || line[0]==' ' || line[0]=='\t') continue;
            const int id=std::atoi(line.c_str());
            if (id>0) banks.insert(id);
        }
        assert(actionOf.at(103)==114 && banks.count(114));
        assert(actionOf.at(119)==84 && banks.count(84));
        assert(actionOf.at(185)==75 && banks.count(75));
        assert(actionOf.at(127)==71 && banks.count(71));
    }
    std::puts("05-O: Characterinfo Action ID binding, monsters, fallback, one-shot death OK");
}
