// 05-W: atributos estaticos da tabela ItemInfo, sem OpenGL e sem servidor.
#include "top/EquipmentStats.h"
#include <cassert>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

static std::vector<std::string> columns(const std::string& line) {
    std::vector<std::string> out;
    std::stringstream f(line);
    std::string c;
    while (std::getline(f, c, '\t')) out.push_back(c);
    return out;
}
int main() {
    using namespace top::equipment_stats;
    assert(firstValue("30,58") == 30);
    assert(firstValue("-2,10") == -2);
    assert(firstValue("abc") == 0);
    assert(firstValue("15000000000000,2") == 0);
    assert(firstValue("0,20") == 0); // nao atribuir por engano a rolagem maxima
    std::vector<std::string> fixture(95, "0,0");
    fixture[52]="2,8"; fixture[53]="3,8"; fixture[54]="4,8";
    fixture[55]="1,8"; fixture[56]="5,8";
    fixture[57]="6,9"; // Lukconstant bonus
    fixture[60]="30,58"; fixture[61]="38,70"; fixture[62]="5,15";
    fixture[63]="40,70"; fixture[64]="20,30";
    fixture[65]="4,5"; fixture[66]="6,7";
    fixture[67]="3,4"; fixture[69]="2,3"; fixture[70]="5,6";
    fixture[71]="50,80"; fixture[73]="7,9";
    const Bonus b = fromColumns(fixture);
    assert(b.str==2 && b.agi==3 && b.dex==4 && b.con==1 && b.sta==5 && b.luk==6);
    assert(b.minAttack==30 && b.maxAttack==38 && b.defense==5);
    assert(b.hp==40 && b.sp==20 && b.dodge==4 && b.hit==6);
    assert(b.critical==3 && b.hpRegen==2 && b.spRegen==5);
    assert(b.moveSpeed==50 && b.resistance==7);
    Bonus sum=b; sum+=b;
    assert(sum.str==4 && sum.hp==80 && sum.resistance==14 && sum.luk==12);
    // Fixture sem colunas suficientes nao deve ler memoria inexistente.
    assert(fromColumns({"100", "test"}).hp==0);
    // Fixtures reais selecionadas do iteminfo.txt: comparacao contra cabecalho
    // historico. Rodam tambem quando Client nao esta instalado na maquina CI.
    for (const char* root : {"Client", "../Client", "../../Client"}) {
        std::ifstream file(std::string(root) + "/scripts/table/iteminfo.txt", std::ios::binary);
        if (!file) continue;
        int found = 0;
        std::string line;
        while (std::getline(file, line)) {
            auto c=columns(line);
            if (c.size()<74) continue;
            if (c[0]=="0281") {
                const auto item=fromColumns(c);
                assert(item.str==2 && item.con==1);
                ++found;
            } else if (c[0]=="0398") {
                const auto item=fromColumns(c);
                assert(item.sta==10 && item.hp==295 && item.resistance==32 && item.defense==43);
                ++found;
            } else if (c[0]=="0418") {
                const auto item=fromColumns(c);
                assert(item.hp==30 && item.moveSpeed==20);
                ++found;
            } else if (c[0]=="0001") {
                const auto item=fromColumns(c);
                assert(item.minAttack==30 && item.maxAttack==38 && item.str==0);
                ++found;
            }
        }
        assert(found==4);
        break;
    }
}
