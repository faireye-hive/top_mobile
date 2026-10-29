# Tales of Pirates offline — Migração 05-AK

## Objetivo
Recuperar a experiência clássica de missões do cliente DX9 usando exclusivamente a UI procedural C++ do novo cliente OpenGL, inspirada nas oito imagens de referência enviadas em 22/10/2026.

## Alterações
- Menu do NPC: até 128 missões históricas extraídas passivamente dos scripts; lista com quatro itens por página e botões Anterior/Próxima.
- Clique em missão abre seus detalhes, com texto inicial, objetivo, itens necessários e recompensas EXP/ouro.
- Estado por opção: disponível, indisponível, ativa ou concluída; missões bloqueadas não permitem aceitar.
- Aceitar continua chamando as regras reais de Play::acceptHistoricalMission, incluindo persistência anterior do save.
- Fechar volta ao jogo; Voltar retorna à lista sem aceitar; a loja e a forja continuam acessíveis pelo diálogo.
- HUD: `!` procedimental próximo aos NPCs com ofertas elegíveis; `?` para missão aceita. Os marcadores são reconstruídos ao trocar de mapa e refletem nível/progresso.
- Diário de missões: novos títulos e cores, progresso existente preservado; botão Abandonar chama Play::abandonHistoricalMission, que salva a mudança e não exclui missões concluídas.
- Nenhum recurso gráfico original reescrito. Nenhuma alteração do formato dos saves. Uma missão histórica ativa por vez, como antes.

## Arquivos centrais
- `Engine/include/top/QuestPresentation.h` — DTO visual e geometria de clique compartilhada.
- `Engine/src/top/Play.cpp` + `Engine/include/top/Play.h` — seleção, validação, salvamento, marcadores e abandono.
- `Platform/linux/main.cpp` — origem dos dados Lua, refresh ao trocar de mapa e cliques do diário.
- `Engine/ui/screens/NpcDialogUI.cpp/.h` — lista e detalhes renderizados proceduralmente.
- `Engine/ui/screens/QuestUI.cpp/.h` — diário de missões.
- `Engine/ui/ActorSkinUIBridge.cpp`, `Engine/include/top/ActorSkin.h`, `Engine/ui/UIManager.h` — passagem do estado passivo à UI.
- `tests/UI05AKTests.cpp` e `Play::selfTest()` — regressões.

## Validação disponível
- Compilação `-fsyntax-only` C++17 de `Play.cpp` e `Platform/linux/main.cpp`: APROVADA.
- `top_ui05ak_tests`: APROVADO.
- Regressão completa CTest headless: **51/51 aprovados** (`0.32 s` no ambiente de validação), com os scripts reais do pacote fornecido.
- Compilação integral do executável OpenGL e comparação visual: **pendentes do Ubuntu do usuário**, porque a máquina de análise não possui `/usr/include/GL/gl.h`.

## Validação sugerida no Ubuntu
```bash
cmake -S sources -B build -DTOP_BUILD_UI_TESTS=ON
cmake --build build -j4
ctest --test-dir build --output-on-failure
./build/top_gl --check
./build/top_gl
```
Visitar um NPC com várias missões (por exemplo, Assistant - Tintin ou Peter, conforme scripts), abrir a lista, visualizar os detalhes, aceitar uma missão disponível, verificar `?`, diário e Abandonar. Também conferir os botões Trade e Forge quando disponíveis. Antes de testar saves, guardar uma cópia de `Client/user/`.

## Limitações documentadas
- O parser Lua é passivo e não executa todas as condições do GameServer. Uma missão listada como disponível pode ainda requerer regras históricas não reconstruídas.
- Apenas uma missão histórica ativa por vez, apesar de haver várias ofertas listadas.
- O menu lista títulos com truncamento visual e um resumo do texto inicial (até três linhas) em 320 × 304; telas menores podem exigir escala/responsividade futura.
- Itens necessários são exibidos por ID na nova janela; o nome localizado pode ser integrado em migração posterior.
- Marcadores procedimentais de NPC são caracteres de fonte, não os sprites 3D originais.
- Os oito mapas experimentais com modelos faltantes permanecem fora da prioridade por escolha do usuário, mas não foram excluídos do catálogo.

## Próxima migração sugerida: 05-AL
Priorizar o aumento seguro de missões históricas simultâneas com status independente, progresso por missão e save versionado retrocompatível. Comparar fórmulas/condições reais de QuestScript com DX9/GameServer; adicionar seleção no diário e objetivos completos com nomes de itens. Preservar o desempenho, as armas já corrigidas, UI original desenhada em C++, forja e os 51 testes atuais.
