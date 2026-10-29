# Tales of Pirates OFFLINE — Migração 05-AR

Data: 22/10/2026. Base **exata**: `TalesOfPirates_05-AQ_Completo.zip`. Próxima: 05-AS.

## Objetivo implementado

Primeiro fluxo histórico entre **dois NPCs**, sem servidor e sem execução de Lua arbitrário: **Blacksmith's Greetings**, registro da missão `701`, oferecida por **Newbie Guide - Senna** e com uma etapa de passagem em **Blacksmith - Goldie**.

Com base no `MissionScript01.lua`:

1. `DefineMission(702, "Blacksmith's Greetings", 701)`: Senna oferece a missão de registro `701` somente após `HasRecord(1)` da Welcome; `MisBeginAction(AddMission,701)`, `SetFlag(701,1)`, `GiveItem(3950,1,4)` fornecem estado inicial e uma carta ao aceitar. Se não houver espaço suficiente para TODOS os itens iniciais, a aceitação falha sem modificar o inventário ou a missão.
2. `DefineMission(703, "Blacksmith's Greetings",701,COMPLETE_SHOW)`: ação auxiliar ligada a `AddNpcMission(703)` de `r_trade01` (Goldie); verifica `NoRecord(701)`, `HasMission(701)`, `NoFlag(701,10)`, `HasItem(3950,1)`, remove o item via `TakeItem(3950,1)` e define `SetFlag(701,10)`. Repetir a conversa sem a carta não repete a ação nem concede a recompensa.
3. O retorno a **Senna** satisfaz `HasFlag(701,10)` e aciona as ações do resultado principal: `ClearMission(701)`, `SetRecord(701)`, `AddExp(9,9)`. A 05-AP já garante entrega única a NPC vinculante. O teste de `Play::selfTest()` verifica o ciclo completo, inclusive recarga de `Q4`.

A etapa auxiliar **NÃO aparece como missão independente**. A implementação permite apenas o subconjunto comprovado: `AlwaysFailure` (impede aceite direto), `NoRecord`, `HasMission`, `NoFlag`, `HasItem`, `TakeItem`, `SetFlag`. Todo resultado desconhecido é recusado. A etapa só é executada se o *NPC escolhido* realmente tiver o seu `displayId` na lista `AddNpcMission()` de seu script. Nenhum evento acontece por aproximação.

## Correção de associação entre IDs

Uma regressão potencial da forma como a UI consultava `npcs.mission(id)` foi corrigida: **`AddNpcMission` usa ID de `DefineMission` (display ID), e o progresso usa `AddMission` (record ID)**. Exemplo concreto: entrada de Senna `702` deve resolver para missão `701` (e não para o registro `702`, que corresponde à etapa seguinte). Adicionado `NpcSystem::missionByDisplay()`, utilizado na montagem de ofertas e marcadores. `NpcSystem::mission(recordId)` continua disponível para testes/consultas por registro e mantém seu comportamento anterior. `COMPLETE_SHOW` não é listado como missão para aceitar.

## Arquivos modificados

- `Engine/include/top/NpcSystem.h`, `Engine/src/top/NpcSystem.cpp`: indexação dos dois tipos de ID; extração passiva de etapas `COMPLETE_SHOW` e itens de início `GiveItem`.
- `Engine/include/top/Play.h`, `Engine/src/top/Play.cpp`: oferta com itens iniciais atômicos; aplicação de ação intermediária com consumo atômico e flags por missão persistentes em `Q4`; testes do fluxo.
- `Platform/linux/main.cpp`: aplicar etapas auxiliares **somente** dos scripts do NPC escolhido; mapear ofertas pelo ID exibido; cabeçalho alterado para 05-AR.
- `tests/UI05ARTests.cpp`: verificações contra os Lua históricos, vínculo Goldie/Senna, IDs e ações. `tests/UI05AQTests.cpp` atualiza o comportamento esperado da missão 701 (agora desbloqueada com a etapa comprovada).
- `CMakeLists.txt`: registra teste 05-AR quando os scripts históricos do cliente estão disponíveis.

## Compatibilidade e limites

- Saves existentes seguem compatíveis. **Nenhum novo campo no save:** `Q2`, `QH2`, `Q3` e `Q4` continuam sem alteração de formato. O progresso intermediário fica em `Q4`, já existente.
- Nenhuma alteração de OpenGL, espadas, poses, combate, UID dos equipamentos, forja, mapas, rede ou assets de `Client/`.
- **Não há emulação geral de `AddTrigger`, `TE_GETITEM`, `TE_KILL` ou `AddNextFlag`**. Esses casos devem permanecer bloqueados quando as flags dinâmicas não puderem ser comprovadas.
- O parser não implementa todos os caminhos `COMPLETE_SHOW` nem premiações auxiliares complexas. Os casos fora do subconjunto seguro não são autorizados.
- Por ora o sistema usa um único `targetX/Y` por missão e `targetId` de abate por vez; quests mais complexas são escopo de outras versões.
- A dica de Goldie na fala Lua menciona Newbie Knife, mas **nenhum item novo foi inventado** fora das ações declaradas no script relevante.

## Validação executada neste ambiente

- `g++ -std=c++17 -fsyntax-only` de `NpcSystem.cpp` e `Play.cpp`: **aprovado**.
- `cmake -S sources -B build-headless -DTOP_BUILD_UI_TESTS=ON`; compilação da meta `top_regression_tests`: **aprovada**.
- `ctest --test-dir build-headless --output-on-failure -j4`: **60/60 testes aprovados**. Foram extraídos somente os scripts/tabelas necessários de `scriptsclientatual.zip` para habilitar os testes históricos. **Essas fixtures não fazem parte do ZIP final do projeto**.
- **O executável `top_gl` da 05-AR não foi compilado nem testado visualmente neste ambiente:** faltam `GL/gl.h` e `libGL.so`. O usuário confirmou no Ubuntu que a 05-AQ compilou, abriu e passou 59/59 testes; isto não constitui validação gráfica da 05-AR.

## Testar no Ubuntu

Fazer backup de `Client/user/` antes de executar. Atualizar apenas a pasta `sources/`; manter toda a pasta `Client/` existente. Após extrair:

```bash
find sources -type f -exec touch {} +  # somente se as datas extraídas estiverem no futuro
rm -rf build
cmake -S sources -B build -DTOP_BUILD_UI_TESTS=ON
cmake --build build -j4
ctest --test-dir build --output-on-failure
./build/top_gl --check
./build/top_gl
```

No jogo, personagem novo em Argent: entregar `Welcome` a Senna; abrir Senna novamente e aceitar `Blacksmith's Greetings`; verificar a carta `3950` no inventário; tentar retornar diretamente a Senna (sem recompensa); falar com Goldie próximo de `(2193,2706)` para entregar carta; verificar indicador `[Pronta]` no diário; salvar/reiniciar; retornar a Senna para receber 9 EXP; confirmar impossibilidade de receber novamente. Testar inventário cheio (não pode aceitar e perder a carta) e saves 05-AQ.

## Próxima etapa — 05-AS

Implementar seletivamente eventos `TE_GETITEM`, `TE_KILL`, `TriggerCondition(IsItem/IsMonster)` e `TriggerAction(AddNextFlag)` conforme as fontes completos do GameServer, com contadores persistentes por missão/flag, protegendo saves e suportando requisitos múltiplos. Preservar os subfluxos de NPC e flags do 05-AR, com testes de missões reais e correlação `displayId`/`recordId`.
