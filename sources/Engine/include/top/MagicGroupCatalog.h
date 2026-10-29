#pragma once
// 05-SK2.2: Group magic [2000,3000), as in the original CMagicEff::CreateGroupMagic.
// magicgroupinfo.txt specifies simple magic IDs, their counts and emission
// mode (0=fan, 1=sequence). It is NOT a magicsingleinfo entry.
#include <algorithm>
#include <cctype>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>
namespace top {
struct MagicGroupDefinition {
    std::vector<int> members; // individual magicsingleinfo IDs, expanded by count
    int motion = 0; // Part_fan=0, Part_sequence=1
};
class MagicGroupCatalog {
public:
    bool load(const std::string& filename) {
        entries_.clear();
        std::ifstream file(filename, std::ios::binary);
        if (!file) return false;
        std::string row;
        while (std::getline(file, row)) {
            if (row.empty() || row[0] == '/' || row[0] == '#' || row.size() > 4096) continue;
            std::vector<std::string> cols;
            std::size_t at=0;
            while (at<=row.size() && cols.size()<8) {
                const auto end=row.find('\t',at);
                cols.push_back(row.substr(at,end==std::string::npos ? end : end-at));
                if (end==std::string::npos) break;
                at=end+1;
            }
            if (cols.size()<6) continue;
            auto parse=[](const std::string& s, int& result) {
                if(s.empty() || !std::all_of(s.begin(),s.end(),[](unsigned char c){return std::isdigit(c)!=0;})) return false;
                try { result=std::stoi(s);return true; } catch (...) {return false;}
            };
            int id=0, types=0, mode=0;
            if (!parse(cols[0],id) || !parse(cols[2],types) || !parse(cols[5],mode) ||
                id<1 || id>999 || types<1 || types>16 || mode>1) continue;
            auto split=[](const std::string& s){
                std::vector<std::string> parts;
                std::size_t at=0;
                while(at<=s.size() && parts.size()<32){
                    auto end=s.find(',',at);
                    parts.push_back(s.substr(at,end==std::string::npos ? end : end-at));
                    if(end==std::string::npos) break;
                    at=end+1;
                }
                return parts;
            };
            const auto ids=split(cols[3]), counts=split(cols[4]);
            if (ids.size()!=static_cast<std::size_t>(types) || counts.size()!=ids.size()) continue;
            MagicGroupDefinition group;
            group.motion=mode;
            bool valid=true;
            for (std::size_t i=0;i<ids.size();++i){
                int magicId=0, number=0;
                if(!parse(ids[i],magicId) || !parse(counts[i],number) ||
                   magicId<1 || magicId>999 || number<1 || number>32 ||
                   group.members.size()+static_cast<std::size_t>(number)>32){valid=false;break;}
                for (int j=0;j<number;++j) group.members.push_back(magicId);
            }
            if(valid && !group.members.empty()) entries_[id]=std::move(group);
        }
        return !entries_.empty();
    }
    const MagicGroupDefinition* find(int id) const {
        auto it=entries_.find(id);
        return it==entries_.end()?nullptr:&it->second;
    }
    std::size_t size() const {return entries_.size();}
private:
    std::unordered_map<int,MagicGroupDefinition> entries_;
};
} // namespace top
