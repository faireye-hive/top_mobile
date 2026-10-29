# Tales of Pirates / migração 05-W — bônus de equipamentos

## Base

Atualização incremental da **05-V**. A 05-V já passou **28/28 testes** e compilou
no GCC 13.3 do usuário. A 05-W não altera interface, mapa, rede, efeitos,
meshes nem o sistema de spawn.

## Defeito corrigido

`Play::loadItems` lia `iteminfo.txt`, mas utilizava apenas as colunas 60, 61 e
62 (ataque mínimo/máximo e defesa). Anéis, amuletos, armaduras e outros itens
com HP, SP, atributos primários ou estatísticas adicionais não alteravam a
ficha de combate.

`EquipmentStats.h` interpreta o **primeiro número** de cada campo `min,max`,
porque os saves offline ainda não possuem as instâncias/rolagens individuais de
forja dos itens. O segundo número é um limite de possível atributo, **não** um
bônus que pode ser aplicado automaticamente. Um campo `0,10` soma **zero**;
isso é intencional para não criar status fictícios.

Campos estáticos suportados de `Client/scripts/table/iteminfo.txt`:

| Coluna | Efeito |
|---|---|
| 52–56 | STR, AGI, DEX, CON, STA |
| 60–62 | ATK mín/máx, DEF (já implementados, preservados) |
| 63–64 | HP máximo, SP máximo |
| 65–66 | esquiva, acerto |
| 67 | crítico (CRT) |
| 69–70 | recuperação HP/SP |
| 71 | velocidade de movimento (cm/s) |
| 73 | resistência física (PDEF) |

Os bônus primários são adicionados **antes** das fórmulas de atributo da
profissão; os bônus diretos depois. Ao equipar ou desequipar, `applyAttrs(false)`
recalcula HP/SP máximos, ataque, defesa, acerto, esquiva, crítico, resistência,
recuperação e velocidade; **não** recupera o HP/SP atual. Desequipar limita os
valores atuais aos novos máximos. A ficha exibe os cinco atributos efetivos,
incluindo equipamentos, sem modificar o layout.

## Exemplos dos dados históricos

- `0281` Final Perseverance: STR +2, CON +1.
- `0398` Armor of Olympus: STA +10, DEF +43, HP +295, PDEF +32.
- `0418` Pendant of Sunstone: HP +30, velocidade +20 cm/s.
- `0001` Short Sword: ATK 30/38; campo STR `0,10` não concede STR em um item sem rolagem.

## Fora do escopo

- Coeficientes percentuais das colunas 29–51.
- Atributos aleatórios/variáveis gerados na criação, refino/forja, gemas,
  sockets, equipamentos com valores próprios persistidos por instância.
- Sets de equipamentos, scripts dinâmicos `ItemEffect.lua`, propriedades
  avançadas de barco, bônus que dependem de eventos do servidor.
- Fórmulas Lua completas de habilidades e estados adicionais.

Não inventar valores para essas funcionalidades; exigem novo modelo de instância
de equipamento persistido por slot, não apenas o `item id` atual do save.

## Testes

- Nova suíte `ui05w_equipment_primary_and_derived_stats` com fixtures
  artificiais e linhas reais de `iteminfo.txt` (quando Client está disponível).
- `ui05v_play_state_regression` foi **recompilado com o Play.cpp da 05-W**;
  `Play::selfTest()` contém cenário de equipar, retirar, restaurar atributos
  base e limitar HP/SP sem curar o personagem.
- 29/29 testes CTest passaram no ambiente headless. As 27 suítes anteriores
  sem alterações foram executadas a partir dos binários aprovados da 05-V.
- Sintaxe C++17 de `Play.cpp` validada com G++.
- Compilação OpenGL completa pendente de teste no Linux do usuário (o ambiente
  desta entrega não dispõe dos cabeçalhos `GL/gl.h`).

## Executar no Linux

```bash
rm -rf build build-ui-tests
cmake -S sources -B build
cmake --build build -j4
./build/top_gl --check
./build/top_gl

cmake -S sources -B build-ui-tests -DTOP_BUILD_UI_TESTS=ON
cmake --build build-ui-tests -j4
ctest --test-dir build-ui-tests --output-on-failure
```

Esperado: banner `top_gl offline - migracao 05-W` e **29/29 testes**.

Verificar equipando `0281`, `0398`, `0418` se já estiverem na bolsa; não é
necessário modificar os saves nem a interface para medir os bônus.
