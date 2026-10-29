# Tales of Pirates OFFLINE — migração 05-AU

Base exata: **05-AT** (`TalesOfPirates_05-AT_Completo.zip`). Objetivo: avançar no processamento dos eventos de missão **sem fundir novamente NPC, UI e motor gráfico**.

## Implementação

- `Engine/include/top/MissionEvents.h`: contrato puro para `TE_KILL` e `TE_GETITEM`; contadores independentes por trigger e emissão *somente da flag final* de `AddNextFlag` ao atingir o total. Sem dependência de NPC, OpenGL, rede ou Lua em tempo de execução.
- `Engine/include/top/MissionTypes.h`: `HistoricalMission::eventTriggers`, com regras coletadas por `MissionCatalog`.
- `Engine/src/mission/MissionCatalog.cpp`: leitura passiva de `MisBeginAction(AddTrigger, triggerId, TE_KILL|TE_GETITEM, targetId, count)`, `InitTrigger`, `TriggerCondition(1, IsMonster|IsItem, targetId)`, `TriggerAction(1, AddNextFlag, recordId, firstFlag, count)` e `RegCurTrigger(triggerId)`. Requer correspondência total dos IDs, contagens, tipos, objetivos `MisNeed` e intervalos de flags sem sobreposição. Se uma condição/result action não for comprovada, a oferta continua bloqueada.
- `Engine/src/mission/PlayMissions.cpp`: função `Play::onHistoricalEvent` despacha eventos para todas as missões ativas, com contadores separados; `historicalObjectivesReady` verifica contadores de kills e as condições de flags/itens. Aceite inicializa os contadores por missão.
- `Engine/src/top/Play.cpp`: combate e coleta enviam eventos *somente após* abates confirmados e itens recolhidos com sucesso. O contador legado de caça permanece para saves antigos; não duplica progresso de missões com gatilhos novos.
- Save opcional **`Q5`** armazena `recordId~triggerId=contagem.triggerId=contagem;`. Os campos anteriores (`Q2`, `QH2`, `Q3`, `Q4`) permanecem nas mesmas posições. Ao carregar, regras são novamente derivadas do Lua original e os contadores são validados por trigger/limite; registros malformados são descartados, nunca instanciam missões. `Q5` é gravado apenas para missões ativas com entrega explícita e gatilhos conhecidos.
- Mensagem de inicialização da versão atualizada para `05-AU`.

## Casos históricos examinados

- **Leaves Collection**, registro 721: `TE_GETITEM` 1573 x10 e 1574 x3 (triggers 7211/7212).
- **Decoction Recipe**, registro 726: três gatilhos de coleta independentes (1777 x2, 1579 x2, 1705 x5).
- **Survival Compass**, registro 736: `TE_KILL` 104 x5, 39 x10 e `TE_GETITEM` 1844 x5; flags finais 14, 29 e 34. **Essa missão continua bloqueada pela condição de profissão `CheckConvertProfession`, não implementada offline**. O reconhecimento dos gatilhos não remove nenhuma exigência de elegibilidade histórica.
- Fontes de verificação: Lua original `MissionScript01.lua` de `scriptsclientatual(2).zip` e rotinas `KillWare`, `MisEventProc`/`GetItem` em `fontes/Server/GameServer/src/Mission.cpp` do `Antigo(4).zip`.

## Garantias/limitações

- Ao cumprir objetivos, **não ocorre entrega prematura a NPC**: o botão Completar da 05-AS continua exigido. Uma missão com objetivos pendentes não paga EXP/ouro/itens. O fluxo Welcome e Senna → Goldie → Senna não foi modificado.
- Progresso de kills e itens é separado por missão/trigger. Duas missões envolvendo a mesma criatura avançam independentemente.
- Contadores intermediários não são materializados como flags individuais em Q4 (apenas a flag final), evitando estourar o limite atual de Q4 e sendo suficiente para os `HasFlag` finais suportados.
- Triggers com parâmetros variáveis Lua, ações condicionais compostas, gametime, mapas, mortes especiais ou chamadas auxiliares continuam **fora do escopo**. Regras Lua não reconhecidas não devem ser presumidas como cumpridas.
- `Play` ainda é uma fachada histórica de jogo, inclusive para a serialização física de `Q5`. Extração completa de um `MissionService` e persistência própria está pendente.
- Saves novos com `Q5` podem perder esses contadores ao serem regravados com binários mais antigos. **Faça backup de `Client/user/`** antes de testar/downgrade. Saves antigos continuam legíveis.
- A lógica `TE_GETITEM` desta etapa é emitida para coleta de loot; **não** é emitida por GM, compras ou itens concedidos por recompensa. Expandir depois somente quando verificadas as rotas históricas.

## Testes realizados (ambiente headless)

- `cmake -S sources -B build -DTOP_BUILD_UI_TESTS=ON` — sucesso.
- `cmake --build build --target top_regression_tests -j4` — sucesso.
- `ctest --test-dir build --output-on-failure` — **64/64 aprovados**, incluindo novo `ui05au_historical_event_triggers` e `Play::selfTest` estendido.
- `Play::selfTest`: duas contagens de itens da missão 721; save/reload `Q5` parcial; NPC incorreto; entrega e recompensa únicas; missão de caça com 2 tipos de monstros e coleta, sem autopremiação.
- `c++ -std=c++17 -fsyntax-only` em `Play.cpp`, `PlayMissions.cpp`, `MissionCatalog.cpp` — sucesso.
- O ambiente do assistente **não possui** `GL/gl.h` e `libGL.so`, portanto a compilação integral / renderização `top_gl` da 05-AU **não foi validada aqui**. O usuário validou a 05-AT no Ubuntu com 63/63, 52 mapas e primeiro quadro OpenGL.

## Testar no Ubuntu

1. Fazer backup de `Client/user/`.
2. Substituir **apenas** `sources/` pelo ZIP 05-AU, preservando `Client/`.
3. Executar:

```bash
cmake -S sources -B build -DTOP_BUILD_UI_TESTS=ON
cmake --build build -j4
ctest --test-dir build --output-on-failure
./build/top_gl --check
./build/top_gl
```

4. Verificar a mensagem `05-AU`, o comportamento dos marcadores, aceitar missões antigas elegíveis, coletar os itens necessários e **usar Completar no NPC**, fechar/reabrir após progresso parcial, testar uma missão de caça se habilitada pelo próprio Lua. Não usar GM para simular evento de coleta.

## Próxima migração sugerida — 05-AV

Expandir com segurança a cobertura de objetivos históricos e gatilhos auxiliares sem liberar condições desconhecidas: tabela de eventos, inspeção das condições `CheckConvertProfession` e das missões com diferentes NPCs/mapas; observar UX de progresso de múltiplos objetivos no diário. Extrair gradualmente serialização e estado da fachada `Play` para contratos de domínio independentes, mantendo compatibilidade CMake e saves.

**Entregáveis**: ZIP integral com `sources/`, patch incremental 05-AT→05-AU, este relatório e `sources/PROMPT_PROXIMA_MIGRACAO.md` atualizado.
