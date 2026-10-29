# Tales of Pirates OFFLINE — migração 05-AV

**Base exata:** 05-AU (`TalesOfPirates_05-AU_Completo.zip`), sem recomeçar. Objetivo: recuperar o progresso visual individual dos gatilhos históricos e tirar a construção do diário do arquivo `Play.cpp`.

## Alterações implementadas

- `Engine/include/top/MissionJournal.h`: modelo de saída `MissionJournalView`, formatação de progresso por `MissionEventTrigger` (abates/coletas), limite defensivo do painel e preservação dos dois últimos campos de entrega ao NPC quando há mais objetivos do que cabem.
- `Engine/src/mission/PlayMissionJournal.cpp`: `Play::missionJournalView()` agrupa estado das missões ativas/concluídas, até 5 entradas por página, 3 missões locais históricas, objetivo, gatilhos `TE_GETITEM`/`TE_KILL` e contadores reais `Q5`, quantidades físicas de itens `neededItems`, estado de entrega e NPC destinatário.
- `Engine/src/top/Play.cpp`: `pushActors` apenas repassa dados montados por `mission/` para `actors.setQuests` e `actors.setQuestCanAbandon`. A lógica do diário saiu do adaptador gráfico.
- `Engine/ui/screens/QuestUI.cpp`: linha `[Pronta]` no diário recebe destaque visual âmbar; nenhuma regra de missão ou OpenGL foi movida para a UI.
- `Platform/linux/main.cpp`: identificação atualizada para `05-AV`.
- `tests/UI05AVTests.cpp`: formatação exata do progresso, saturação dos contadores, limites da lista e preservação do nome do NPC na região visível do diário.
- `Play::selfTest()`: verifica que a missão histórica 721 exibe separadamente as coletas `1573 6/10` e `1574 1/3` antes e após recarregar Q5, e que múltiplos tipos de inimigos e coleta aparecem independentemente.
- `CMakeLists.txt`: `PlayMissionJournal.cpp` vinculado aos alvos que compilam `Play.cpp`; teste 05-AV incluído no `top_regression_tests`.

## Comportamento preservado

- Sem alteração no formato do save, Q2/QH2/Q3/Q4/Q5 ou na interpretação dos scripts Lua.
- Não há entrega automática, recompensa nova ou execução de Lua arbitrário.
- O botão **Completar**, os marcadores de NPC e o limite de oito missões ativas permanecem inalterados.
- As três missões locais continuam listadas após as missões históricas.
- Para contagem de coleta, a linha `Coletar` mostra os eventos obtidos após aceitar a missão, que podem diferir do conteúdo atual da bolsa; `Entregar` mostra a quantidade **física** na bolsa. Isto é intencional.
- O painel é pequeno: até 9 linhas de detalhe mais a linha de paginação. Se exceder, resume as intermediárias com `... +N objetivos`, preservando o destino de entrega ao NPC. **Ainda não existe paginação independente da seção de objetivos.**
- Missões com gates/condições históricas não interpretadas, como `CheckConvertProfession` na missão 736, continuam bloqueadas.

## Validação no ambiente do assistente

- Checagens `g++ -std=c++17 -fsyntax-only` de `Play.cpp`, `PlayMissionJournal.cpp` e `QuestUI.cpp`: aprovadas.
- CMake configure e build headless `top_regression_tests`: aprovados após recompilação incremental.
- `ctest --test-dir build --output-on-failure`: **65/65 testes aprovados**, incluindo `ui05av_modular_mission_journal` e `ui05v_play_state_regression`.
- **Não foi compilado nem renderizado `top_gl` neste ambiente:** faltam `GL/gl.h` e biblioteca de link OpenGL. A 05-AU foi validada no Ubuntu do usuário com 64/64 testes, `top_gl` e primeiro quadro. O usuário deverá validar 05-AV no Ubuntu.

## Próxima etapa e roteiro de funcionalidades

O roteiro desta etapa foi substituído por [ESTADO_DO_PORTE.md](ESTADO_DO_PORTE.md). A intenção do usuário é: **missões → utilidades dos itens e separação do módulo item → revisão histórica de stats → revisão das skills e animações/efeitos**. Avançar incrementalmente, sem substituir o inventário UID, a forja ou os sistemas de combate já funcionais.

## Teste no Ubuntu

1. Backup obrigatório de `Client/user/`.
2. Substituir **somente** `sources/` (não incluir assets Client no ZIP).
3. Rodar `cmake -S sources -B build -DTOP_BUILD_UI_TESTS=ON`, `cmake --build build -j4`, `ctest --test-dir build --output-on-failure`, `./build/top_gl --check` e `./build/top_gl`.
4. Conferir `05-AV` na inicialização; diário para missões com duas ou três coletas/caças; status verde/amarelo, `Aceitar` e `Completar`, Welcome e Blacksmith's Greetings; progresso após salvar/reabrir.
