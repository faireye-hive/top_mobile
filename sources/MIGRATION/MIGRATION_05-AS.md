# Tales of Pirates OFFLINE — Migração 05-AS

Data: 22/10/2026. Base exata: `TalesOfPirates_05-AR_Completo.zip`. Próxima: **05-AT**.

## Objetivo desta etapa

Recuperar a experiência de aceitar/entregar missões **pela janela do NPC**, conforme as capturas do cliente DX9 fornecidas: oferta com indicador **`!` verde**, missão que pode ser entregue com indicador **`?` amarelo**, detalhes com botão **Aceitar** ou **Completar**, várias missões disponíveis no mesmo NPC e recuperação de missões de níveis anteriores no offline. A interface permanece procedural C++/OpenGL, sem recursos DirectX e sem reproduzir literalmente as artes DX9.

## Alterações

- `Platform/linux/main.cpp`: removidas as entregas automáticas `deliverWelcomeToSenna()` e `deliverHistoricalToNpc()` da simples interação `E`. O jogador precisa selecionar a missão e clicar em **Completar**. As ações auxiliares históricas de NPC (`COMPLETE_SHOW`, Goldie 703) ainda ocorrem quando se conversa com o NPC correto, pois fazem parte de uma etapa intermediária e não são a conclusão definitiva da missão. O texto de inicialização identifica `05-AS`.
- `Engine/include/top/QuestPresentation.h`: novo estado passivo `NpcQuestView::Ready=4`, mantendo os valores anteriores.
- `Engine/src/top/Play.cpp` e `Play.h`: missões aceitas e atribuídas ao NPC da conversa aparecem junto das ofertas, mesmo quando a lista de ofertas novas ficou vazia. `Welcome` recebida por personagem novo aparece explicitamente em Senna e exige **Completar**. `missionReadyAtNpc()` verifica NPC, estado ativo, objetivos conhecidos e espaço na bolsa. `clickDialog(5)` executa a entrega apenas para o ID da entrada selecionada, impedindo entregar duas missões com um clique. `deliverHistoricalToNpc(nome, id)` aceita ID opcional para seleção precisa; chamadas legadas com apenas nome continuam válidas para testes antigos. Após a entrega o diálogo fecha; a recompensa só ocorre uma vez. Indicadores de mundo priorizam amarelo para entrega, depois verde para oferta, depois acompanhamento `?`.
- `Engine/ui/screens/NpcDialogUI.cpp`: detalhes passam a exibir **Pronta para entregar** e botão **Completar**, conservando Aceitar para novas missões; itens e recompensas continuam mostrados no painel procedural.
- **Missões antigas**: no modo offline, desconsideramos o *limite máximo de nível* como regra de expiração das ofertas (`kAllowPastLevelQuests=true`). Ainda são respeitados o **mínimo de nível**, profissão, itens, histórico de missões, condições desconhecidas (falha fechada), missão já concluída, máximo de 8 missões simultâneas e 128 registros extras no save. Esta política de *catch-up* é uma decisão de design offline: **difere dos `LvCheck("<", ...)` encontrados em alguns Lua originais**. Por exemplo, no Lv10 pode aceitar uma missão antes limitada ao Lv5–6, desde que os demais requisitos estejam satisfeitos.
- Nenhuma alteração nos campos do save: `Q2`, `QH2`, `Q3` e `Q4` inalterados. Continua compatível com saves antigos. Nenhum arquivo `Client/` ou `Client/user/` incluído/substituído.

## Testes

- `g++ -std=c++17 -fsyntax-only` de `Play.cpp`, `NpcDialogUI.cpp` e `Platform/linux/main.cpp`: aprovados.
- Novo `tests/UI05ASTests.cpp` verifica o desenho do botão **Completar**, estado pronto e **Aceitar**. Registro `ui05as_explicit_quest_completion_ui` no CMake.
- `Play::selfTest()` estendido para verificar missão antiga com nível excedido, aceite, estado pendente, botão Completar quando pronta, recompensa única, Welcome com botão e múltiplas missões prontas no mesmo NPC (completar só a selecionada). Também verifica recarga do progresso pendente da outra missão.
- `cmake -S sources -B build-headless -DTOP_BUILD_UI_TESTS=ON` e `cmake --build build-headless --target top_regression_tests -j4`: aprovados com fixtures históricas de `scriptsclientatual(2).zip` em `Client/` **apenas no ambiente de teste**.
- `ctest --test-dir build-headless --output-on-failure -j4`: **61/61 testes aprovados**, sem falhas.
- **Não** foi possível compilar ou executar o executável gráfico `top_gl` neste ambiente por ausência de `GL/gl.h` e `libGL.so`; compilação/teste visual precisam ser realizados no Ubuntu do usuário.

## Limitações conhecidas

- Ainda não foram migrados todos os `TE_GETITEM`, `TE_KILL`, `AddNextFlag` e gatilhos de localização/eventos do GameServer. Algumas missões que dependem desses gatilhos continuam bloqueadas. Em particular, a representação de visita a coordenadas para missões com entrega é parcial; não presumir que todos os objetivos compostos já funcionam.
- O sinal acima do NPC utiliza fonte procedural e cores, **não** o sprite ou moldura pixel-perfect do cliente DirectX original. Uma aproximação artística completa requer etapa visual posterior.
- Os indicadores ficam visíveis dentro do raio de renderização já existente; o menu de missões por mapa segue disponível. Não há novos objetivos inventados nem novas recompensas.

## Testar no Ubuntu

1. Fazer backup de `Client/user/`; extrair o ZIP 05-AS na raiz, substituindo somente `sources/`.
2. `cmake -S sources -B build -DTOP_BUILD_UI_TESTS=ON && cmake --build build -j4 && ctest --test-dir build --output-on-failure`.
3. `./build/top_gl --check` e `./build/top_gl`: confirmar 05-AS, mapa, personagens, indicadores e primeira cena.
4. **Novo personagem**: falar com Senna: a Welcome **não** é entregue automaticamente; escolher Welcome e clicar **Completar**, receber 6 EXP apenas uma vez.
5. Conversar novamente com Senna: aceitar Blacksmith's Greetings, receber item 3950, ir a Goldie e entregar a carta, voltar à Senna: **? amarelo**; selecionar a missão, clicar **Completar**, ganhar **9 EXP**. Repetir a conversa não premia novamente.
6. Testar NPC com várias ofertas e confirmar que Aceitar/Completar atua apenas sobre a missão selecionada; reiniciar para confirmar persistência. Testar missões de nível anterior com pré-requisitos concluídos e bolsa disponível.
7. Caso apareçam avisos `Clock skew detected`, conferir `timedatectl status` e usar `find sources -type f -exec touch {} +`. Ao trocar de versão pode remover apenas `build/`, nunca `Client/`.

## Próxima migração 05-AT

Implementar os gatilhos originais `TE_GETITEM`/`TE_KILL`/`AddNextFlag` e controle de destino, com progresso por objetivo e persistência defensiva. Depois melhorar legibilidade e aparência das telas de detalhes da missão, mantendo o fluxo explícito de aceite/entrega introduzido aqui.
