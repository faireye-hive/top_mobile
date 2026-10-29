# Tales of Pirates offline — Migração 05-AP

Data: 22/10/2026. Base exata: `TalesOfPirates_05-AO_Completo.zip` enviado nesta conversa.

## Escopo concluído

Implementada **primeira etapa conservadora** de entrega de missões históricas a NPCs, preservando gameplay offline em C++17 e UI procedural do OpenGL.

1. `NpcSystem::mission()` agora identifica `MisResultCondition` e `MisResultAction` dos blocos `DefineMission` indexados. Reconhece `HasMission(registro)`, `NoRecord(registro)`, `HasItem(item, quantidade)` em condições e `TakeItem`, `ClearMission(registro)`, `SetRecord(registro)`, `AddExp`, `AddMoney` e `GiveItem` em ações. **Não executa Lua**. Só atribui `turnInRequired=true` quando comprova `HasMission` e `ClearMission` do mesmo ID de registro (`AddMission`, não o ID de exibição), e quando `TakeItem`/`HasItem` concordam com `MisNeed(MIS_NEED_ITEM)`. Se há resultado explícito contendo operações/condições não modeladas, a oferta é bloqueada com erro conservador, em vez de prometer recompensa indevida. As missões sem resultado explícito e o fluxo legado de 05-AO preservam sua lógica anterior.
2. No aceite via janela real de NPC, a instância de missão grava o nome exato do NPC que a ofereceu como destinatário da entrega. Missões assim marcadas param de ser autoentregues pelo frame de movimento, coleta e contagem de kills. O diário exibe `[Pronta]` quando os objetivos verificáveis estão completos e indica o NPC destinatário.
3. Em `Platform/linux/main.cpp`, a interação **já validada pela distância** chama `Play::deliverHistoricalToNpc`. Só conclui missões prontas e entregues ao NPC cujo nome corresponde exatamente ao vínculo guardado. A pré-simulação do inventário em `missionRewardsFit` evita entregar quando os consumos/recompensas não cabem; `completeHistoricalMission` retira materiais, concede itens/EXP/ouro, altera estado e salva uma única vez. A Welcome mundial 1/Senna conserva o fluxo especial da 05-AO.
4. Formato do save: novo campo final **`|Q3:id~HEX_NPC;...`** (somente missões ativas com entrega comprovada). Campos antigos não foram reordenados ou sobrescritos. `Q2` e `QH2` permanecem legíveis; saves antigos, sem `Q3`, seguem funcionando. Campos Q3 malformados são descartados. Regravar um save da nova versão pela 05-AO (downgrade) perde o vínculo de entrega Q3.
5. Não houve mudança na renderização, modelos, equipamentos/espadas, forja, inventário por UID, habilidades, portais nem mapas.

## Provas nos arquivos históricos (sem inferir conteúdo ausente)

- `scripts/offline/script/MisScript/MissionScript01.lua`: `DefineMission(733,"Leaves Collection",721)`, `MisNeed(MIS_NEED_ITEM,1573,10)` e `(1574,3)`, `MisResultCondition(HasMission,721)`, `HasItem`, `MisResultAction(TakeItem,...), AddExp(40,70), ClearMission(721)`; também `MisPrize(MIS_PRIZE_MONEY,100,1)`. O parser reconhece entrega ao NPC, consumo desses 13 itens, 40 EXP **(limite inferior legado da 05-AO, ver limitações)** e 100 de ouro.
- Mesmo script: `DefineMission(738,"Decoction Recipe",726)` com itens 1777 ×2, 1579 ×2, 1705 ×5, entrega, `AddExp(70,95)` e prêmio monetário 200.
- `NpcScript01.lua` lista `AddNpcMission(721)` e `AddNpcMission(726)`; a UI mantém a oferta conforme o NPC vinculado.
- O fonte antigo `fontes/Server/GameServer/src/Mission.cpp` distingue `MisSetRecord` de `MisClearRecord` e da própria missão. **Ainda não implementamos um sistema autônomo de registros e flags**: para `SetRecord` idêntico ao registro da missão concluída, mantém-se a representação herdada da 05-AO via histórico de conclusão.

## Testes realmente executados

- **58/58 testes headless aprovados** em `ctest --test-dir build-headless --output-on-failure -j4`, incluindo as 57 regressões anteriores e `ui05ap_historical_npc_turn_in`.
- `Play::selfTest()` ampliado: duas missões simultâneas, material coletado, morte de monstros, missão incompleta, NPC errado, NPC certo, itens/EXP/ouro uma vez, bloqueio de duplicação após reload, preservação de missão concluída e simulação de save anterior sem Q3.
- `UI05APTests.cpp`: carrega os Lua originais para verificar `Leaves Collection` (registro **721**, apesar de `DefineMission(733)`) e `Decoction Recipe` (registro **726**, apesar de `DefineMission(738)`), bloqueando fluxo com flags ainda não implementadas.
- `g++ -std=c++17 -fsyntax-only -IEngine/include` para `NpcSystem.cpp`, `Play.cpp`, `ActorSkinUIBridge.cpp`, `Platform/linux/main.cpp` e o teste novo: aprovado.
- **Não foi compilado nem executado integralmente `top_gl`** no ambiente do assistente: ausentes `GL/gl.h` e `libGL.so`. Nenhum FPS, posição visual ou UI em execução é reivindicado para a 05-AP.

## Restrições e próximos passos (05-AQ)

- `HasFlag`, `NoFlag`, `SetFlag`, triggers, missões com **mais de um tipo de monstro** e diferentes ramificações de entrega, opções de recompensa e ações de profissão/teleporte ainda precisam de interpretação e persistência própria. Missões com tais operações em resultados explícitos ficam ocultas do oferecimento até existir suporte seguro.
- As rotas provadas nesta etapa associam a entrega ao **NPC que ofereceu a missão**. Missões históricas que exigem um destinatário diferente ainda necessitam de mapeamento do fluxo original e não devem ser habilitadas automaticamente.
- `AddExp(inferior,superior)` preserva a **primeira quantidade** que a 05-AO já utilizava (ex.: 40 no caso 40,70); o intervalo aleatório do servidor original ainda não foi reproduzido. `SetRecord` segue representado apenas por missão concluída, sem flags autônomas. Os 3 objetivos locais continuam com o comportamento anterior. Não inferir equivalência integral com o servidor antigo.
- A validação de inventário e recompensa utiliza as rotinas pré-existentes e não muda a semântica histórica de bônus ou seleção; confirmar casos mais complexos com as fontes do servidor antes de ampliar.

## Validar no Ubuntu do usuário

Fazer backup de `Client/user/` antes de usar saves. Extrair **somente `sources/`** do ZIP novo na raiz do projeto, mantendo intactos `Client/` e `Client/user/`. Os scripts originais devem estar em `Client/scripts/`.

```bash
cmake -S sources -B build -DTOP_BUILD_UI_TESTS=ON
cmake --build build -j4
ctest --test-dir build --output-on-failure
./build/top_gl --check
./build/top_gl
```

Em um personagem apto à missão `Leaves Collection` ou `Decoction Recipe`, conversar com NPC ofertante, aceitar, obter materiais e verificar que **não** há prêmio automático; conferir `[Pronta]` no diário, testar outro NPC e por fim conversar com o mesmo NPC que ofereceu. Verificar ouro, inventário, EXP, salvar/reabrir e tentar interagir novamente. Verificar paralelamente a Welcome a Senna com personagem novo em `garner`. **Estas validações gráficas e interativas ficam a cargo do usuário no Ubuntu**.

Artefatos: ZIP completo 05-AP, patch verificado 05-AO→05-AP, documento presente e `sources/PROMPT_PROXIMA_MIGRACAO.md` atualizado.
