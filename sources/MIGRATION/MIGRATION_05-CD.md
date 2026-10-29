# 05-CD — Separação do núcleo de jogo e regras de combate

## Modularização

`Play.cpp` concentrava o gameplay e mais de 3.300 linhas de autotestes, chegando a 7.512 linhas. Os testes passaram para `PlaySelfTest.cpp`; fórmulas puras de combate foram separadas em `PlayCombatRules.h/.cpp`, sem dependência de UI ou OpenGL. O método `Play::selfTest()` permanece como ponto de entrada dos diagnósticos. CMake compila os módulos tanto no executável Linux quanto no alvo de regressão.

O primeiro corte ainda deixava `Play.cpp` com 4.063 linhas. Nesta etapa, o código de produção também foi organizado em `Engine/src/top/play/`: `PlayCombat.cpp` reúne combate/habilidades/IA, `PlayWorld.cpp` seleção e estado do mundo, `PlayProgression.cpp` fórmulas de atributos/classe, `PlayEconomy.cpp` loot/loja/forja/inventário e `PlayPresentation.cpp` composição de dados para atores e HUD. As fórmulas puras seguem em `PlayCombatRules.h/.cpp`; `PlaySelfTest.cpp` continua isolado.

Contagem atual: `Play.cpp` 1.583 linhas; os módulos novos têm 211–1.108 linhas. Isso elimina o núcleo de gameplay monolítico sem espalhar estado em variáveis globais: cada método continua operando sobre a mesma fachada `Play`. `main.cpp` ainda tem 3.132 linhas e é uma próxima frente de decomposição do cliente Linux; não foi alterado nesta etapa para evitar misturar a refatoração do loop GLX/eventos com a mudança dos módulos de gameplay.

As regras continuam cobertas por `Play::selfTest`, pelo alvo `top_ui05v_play_tests` e pela suíte existente; os testes não foram duplicados nos módulos de produção.

## Altura naval histórica

Conferi `CCharacter::_UpdateHeight()` em `arquivo/fontes/Client/src/Character.cpp`: atores no território SEA usam `sSeaHeight / 100`, não a altura do fundo do mar. A tabela ligada por `shipinfo.cha list ID` aponta os 17 barcos selecionáveis para personagens de território SEA com `SeaHeight = -10`, ou seja, origem a −0,10 m do nível da água. O diagnóstico de inicialização agora verifica esse valor em todos os 17 cascos, além de abrir/desmontar cada modelo. A altura permanece ligada ao corpo embarcado e à câmera.

Esse ajuste não altera a física da água ou os modelos; a origem/altura segue o dado do cliente DirectX. Se a cena ainda mostrar casco afundado depois dessa verificação, a próxima medição é a linha d'água relativa aos vértices de cada modelo, não inventar outro deslocamento global.

Validação: `top_gl --check`, `top_ui05v_play_tests`, CTest 71/71 e `git diff --check` aprovados. Nenhum asset novo foi copiado de `arquivo/` neste incremento.
