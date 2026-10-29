// 05-AE: o menu historico do NPC e codigo Lua lido como dado, nao executado.
#include "top/NpcForgeServices.h"
#include <cassert>
#include <iostream>
#include <string>
using namespace top;

static void expect(const std::string& lua, unsigned services, bool expectedFound = true,
                   const std::string& name = "r_trade01") {
    bool found = !expectedFound;
    assert(readNpcForgeServices(lua, name, &found) == services);
    assert(found == expectedFound);
}
int main() {
    expect(R"lua(
        function r_trade01 ()
          Talk(1, "Blacksmith - Goldie")
          Text(1, "Buy", BuyPage)
          Text(1, "Forge", OpenForge)
          Text(1, "Combine", OpenUnite)
          Text(1, "Socket", OpenMilling)
        end
        function r_trade02 ()
          Text(1, "Forge", OpenForge)
        end
    )lua", 7u);

    // Nem comentarios, nem texto de etiqueta nem prefixos de nomes sao codigo.
    expect(R"lua(
        -- function r_fake() Text(1,"Forge",OpenForge) end
        function r_trade01()
          -- Text(1,"Forge",OpenForge)
          --[[ Text(1,"Combine",OpenUnite) ]]
          --[=[ Text(1,"Socket",OpenMilling) ]=]
          Talk(1,"Text(1, 'Forge', OpenForge) -- frase")
          Text(1,"OpenForge",BuyPage)
          Text(1,"OpenMilling",OpenUniteExtended)
          Text(1,"-- Repair",OpenRepair)
          Text(1,"OpenUnite",NoOp)
          Text(1,"-- literal", OpenForgeExtended)
          Text(1,"OpenForge","OpenForge")
        end
    )lua", 0u);

    expect(R"lua(
        function r_trade01_extra() Text(1,"Forge",OpenForge) end
        function r_trade02() Text(1,"Combine",OpenUnite) end
    )lua", 0u, false);

    // Chamadas com espacamento, multi-linhas e comandos condicionais.
    expect(R"lua(
        function r_trade01 ( )
          if enable_shop then
            Text (
                1, "Forge", OpenForge
            )
          end
          for i=1,3 do
            if i>1 then Text(1,"Combine",OpenUnite) end
          end
          Text(1,"Socket",OpenMilling) -- OpenForge
        end
    )lua", 7u);

    // Funcoes internas nao viram opcoes do menu do NPC.
    expect(R"lua(
        function r_trade01()
          local function hidden_menu()
            Text(1,"Forge",OpenForge)
          end
          Text(1,"Combine",OpenUnite)
        end
    )lua", kNpcCombine);

    expect(R"lua(
        function r_trade01()
          Text(1,"Forge",OpenForge) -- valido
          Obj.Text(1,"Socket",OpenMilling)
          Text(1,"Combine",method(OpenUnite))
          Text(1,"Socket",something.OpenMilling)
          Text(1,"Socket",OpenMilling,Other)
          Text(1,"Forge",OpenForge + 1)
        end
    )lua", kNpcForge);

    // Strings longas sao tratadas literalmente; funcao truncada nao habilita.
    expect(R"lua(
        function r_trade01()
          Talk(1, [=[ Text(1,"Forge",OpenForge) ]=])
          Text(1,"Combine",OpenUnite)
        end
    )lua", kNpcCombine);
    expect("function r_trade01() Text(1, 'Forge', OpenForge)", 0u, false);
    expect("function r_trade01()\nText(1, 'Forge', OpenForge)\nend", kNpcForge);

    std::cout << "05-AE: lexing Lua, isolamento do NPC e argumentos Text() OK\n";
}
