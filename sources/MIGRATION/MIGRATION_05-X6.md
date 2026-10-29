# Tales of Pirates — 05-X6: frustum culling de atores e diagnostico GLX/VSync

**Base:** 05-X5. **Alvo:** cliente historico offline C++17/OpenGL no Linux. Sem alterar saves, habilidades, IA, mouse, camera, equipamentos, efeitos e streaming da 05-X1/05-X3.

## Motivacao e resultados anteriores

O perfil real da 05-X5 com Ryzen 5 4600G mostrou ~75 FPS frequentes, `atores` em torno de 4–6 ms e cache de atores economizando ~2 MiB/quadro, sem ganho consistente de FPS. A 05-X6 busca evitar *skinning* e envio de vertices de personagens inteiramente fora do campo da camera; nenhum ganho de FPS e alegado sem a medicao no Linux.

## Frustum culling

`Engine/include/top/FrustumCulling.h` extrai e normaliza os seis planos (esquerda/direita, cima/baixo, perto/longe) da matriz OpenGL **projection * view**, em formato column-major. Teste esfera × plano: so descarta quando a esfera inteira esta fora. Matrizes invalidas permitem a passagem (*fail-open*).

`ActorSkin::setFrustum` recebe a matriz produzida **uma unica vez**, antes do `build()`, exatamente a mesma usada para renderizar terreno/objetos/personagens. A margem de seguranca e conservadora: raio de pelo menos **8 metros**, maior em modelos com sombra larga/comprida. O jogador **nunca e descartado**.

O `ActorSkin` continua atualizando a pose logica, contador de quadros, posicao, yaw e altura de todos os modelos elegiveis antes da filtragem. Apenas a montagem **visual** (matrizes osseas, skinning, sombras, barras, vertex batches) e pulada quando a esfera fica totalmente fora. IA, combate, hit-testing, spawns, quests e saves continuam no `Play` e `NpcSystem`, fora do culling. Ao retornar ao campo de visao, a pose segue o tempo atual. O culling se aplica aos modelos 3D, nao ao mundo inteiro nem aos marcadores de portal.

```
./build/top_gl --profile                      # frustum ON (padrao)
./build/top_gl --profile --no-frustum-culling # comparacao A/B, OFF
```

O perfil inclui:

```
profile frustum atores: testados ... | visiveis ... | descartados ... | ON/OFF
```

`testados` conta modelos nao ocultos que chegaram ao renderizador, `visiveis` inclui o jogador e `descartados` sao modelos fora dos seis planos. Se o jogador girar a camera, a contagem deve mudar; modelos 3D voltam a aparecer ao entrar em tela. Um monstro longe da camera continua lutando/andando normalmente.

## Investigacao de 75 FPS e VSync

**O codigo nao contem `sleep` nem um limitador explicito de 75 FPS.** O numero repetido pode corresponder ao refresh da tela, configuracao do driver, compositor, ou simplesmente ao custo do frame. Nao foi demonstrado qual deles atua na maquina do usuario.

`GlxWindow` agora consulta `GLX_EXT_swap_control` quando presente e imprime o intervalo informado pelo driver. Por padrao **nao modifica** o VSync anterior (politica do driver/compositor). A mudanca e opcional para experimentos:

```
./build/top_gl --profile --vsync on
./build/top_gl --profile --vsync off
```

Se a extensao suportada nao permitir mudanca, o jogo exibe aviso e segue normalmente. `GLX_MESA_swap_control` e um fallback para alterar; sem EXT nao existe consulta confiavel ao intervalo atual. Mesmo com VSync OFF, o compositor pode impor sua propria sincronizacao. Desativar VSync pode causar *tearing*: usar apenas para medir, depois voltar a ON/padrao.

**Compare no mesmo mapa, camera, local e numero aproximado de NPCs/monstros.** Se `--vsync off` ultrapassar ~75 FPS, ha um limitador externo relacionado a apresentacao. Se nao ultrapassar, meca `atores`, `stream`, `render`, `swap` para entender o limite; nao concluir automaticamente que o hardware e insuficiente.

## Build/testes

```
cmake -S sources -B build && cmake --build build -j4
./build/top_gl --check
./build/top_gl --profile
cmake -S sources -B build-ui-tests -DTOP_BUILD_UI_TESTS=ON
cmake --build build-ui-tests -j4
ctest --test-dir build-ui-tests --output-on-failure
```

O teste novo `ui05x6_actor_frustum_and_clip_planes` valida os planos da matriz OpenGL, esfera parcialmente na borda, lados/costas, plano proximo e distante, transformacao de camera e invalidacao segura. **38/38 CTest passaram** neste ambiente. O teste integrado de `Play` foi recompilado e passou; os demais 36 testes anteriores reutilizaram os executaveis previamente aprovados 05-X5, com fonte nao alterada. Os arquivos principais alterados `ActorSkin.cpp` e `main.cpp` passaram `g++ -fsyntax-only` em C++17; a parte `GlxWindow.cpp` passou com declaracoes de teste GLX devido a ausencia dos headers oficiais OpenGL. Nao foi executado o aplicativo grafico neste ambiente: compile e teste visualmente no Linux do usuario.

Confirme no jogo: NPCs nas bordas nao piscam, monstros reaparecem com pose correta ao girar a camera, sombras/equipamentos continuam alinhados, personagem sempre renderizado, e clicar no monstro fora do eixo de camera ainda funciona quando estiver na tela.
