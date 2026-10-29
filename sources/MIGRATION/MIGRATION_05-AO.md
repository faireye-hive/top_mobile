# Tales of Pirates offline — Migração 05-AO

Data: 22/10/2026. Base: 05-AM. Objetivo: sequência inicial Welcome e descoberta de missões por mapa, sem modificar as regras de equipamentos e o renderizador.

## Dados recuperados dos Lua históricos

- `Client/scripts/offline/script/MisScript/EudemonScript.lua`: `DefineWorldMission(1, "Welcome", 1)`, destino Newbie Guide - Senna (2223, 2785), em Argent City.
- `Client/scripts/offline/script/MisScript/MissionScript01.lua`: `DefineMission(701, "Welcome", 1, COMPLETE_SHOW)` entrega a missão 1 e executa `ClearMission(1)`, `SetRecord(1)`, `AddExp(6,6)`.
- `Client/scripts/offline/calculate/variable.lua`: `DEXP[1]=0`, `DEXP[2]=5`, portanto 6 EXP elevam um personagem recém-criado do nível 1 ao 2.
- A execução **não** interpreta arbitrariamente essas rotinas Lua: implementamos a regra passiva específica e conhecida da Welcome. A ação `SetRecord(1)` é representada pelo registro de missão histórica concluída ID 1, usado pelo índice de requisitos local, não por um sistema geral de flags Lua.

## O que mudou

1. Personagem novo criado no mapa `garner`, sem save anterior, inicia `Welcome` (missão mundial ID 1) no diário histórico. Saves existentes não recebem uma missão retroativamente. O novo save é criado após a inicialização da ficha.
2. Chegar perto do local não conclui a Welcome; somente a **conversa dentro do alcance com Newbie Guide - Senna** conclui, concede 6 EXP e grava o progresso. A entrega é idempotente, sem prêmio repetido.
3. Ofertas indisponíveis são **omitidas da lista** na conversa. Os requisitos continuam guardados internamente e são verificados outra vez ao aceitar; isso impede burlar o filtro por chamadas diretas.
4. Os marcadores de NPCs utilizam `!` verde para ofertas elegíveis, `?` para missão ativa. Não indicam missões bloqueadas como disponíveis.
5. Botão **`! Missoes do mapa`** no canto inferior esquerdo. Painel próprio desenhado no motor, com 7 entradas/página, nome da missão, NPC, coordenadas históricas e distância aproximada da posição do jogador. A seleção destaca a linha; navegação por páginas e fechamento possuem hitboxes testáveis. O painel mostra somente as ofertas elegíveis do **mapa atual**.
6. A renderização OpenGL, UID/equipamentos, forja, NPCs, portais, combate e esquema de saves anteriores não foram alterados.

## Verificações realmente executadas neste ambiente

- `g++ -std=c++17 -fsyntax-only` em `main.cpp`, `Play.cpp` e `ActorSkinUIBridge.cpp`: passou.
- Regressão `Play::selfTest()`: confirmação da entrega a Senna, 6 EXP, nível 2, bloqueio de repetição, lista de mapa com pré-requisitos e ocultação de missões. Passou.
- `ui05ao_map_quest_discovery_layout`: botão inferior, linhas, paginação, fechamento, bloqueio de clique no terreno. Passou.
- `ctest`: **57/57 aprovados**, com Client/scripts histórico disponibilizado na pasta de build.
- Compilação integral `top_gl` não executada com sucesso aqui: ambiente sem `GL/gl.h`. Precisa compilar/testar o visual em Ubuntu com OpenGL.

## Instalação / testes no Ubuntu

Fazer backup de `Client/user/` antes de testar. Extraia o ZIP completo sobre a raiz do projeto, mantendo `Client/` histórico.

```bash
cmake -S sources -B build -DTOP_BUILD_UI_TESTS=ON
cmake --build build -j4
ctest --test-dir build --output-on-failure
./build/top_gl
```

Criar **personagem novo** em Argent City, verificar `Welcome` no diário, aproximar-se de Senna e interagir: nível 2 após o prêmio e registro salvo depois de reiniciar. Em NPCs com diversas missões, confirmar ocultação das bloqueadas; testar `! Missoes do mapa` e o nome/coordenadas de NPCs.

## Limitações honestas / próximos passos

- A sequência mundial Welcome fora de Argent (Resline/Angela) não foi migrada nesta etapa.
- Entrega de outras missões aos NPCs, `MisResultCondition`/`MisResultAction` gerais e flags complexas ainda precisam de implementação (05-AP). `HasRecord` atualmente se apoia no histórico de missões concluídas.
- O localizador mostra posição e distância, mas não configura navegação automática/rota e não procura missões de mapas vizinhos.
- A oferta continua limitada ao subconjunto de objetivos e condições históricas que o cliente offline sabe avaliar; missões com condições desconhecidas não são expostas.
