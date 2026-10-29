# Migração 05-N — Navegação de monstros por A* local

Base: 05-M. Código C++17 offline, sem modificações na UI ou rede.

## Implementação

- Reaproveita o modelo de custo A* octile e a grade de 50 cm do `PathFinder`.
- Nova `PathFinder::findLocalGrid` oferece grade simulável em testes; `findLocal` aplica `MapFile::blockedAt` com tamanho real do mapa.
- A busca limita deslocamento a 32 m, visita no máximo 5.000 nós, aloca no máximo 12.500 células e restringe a rota ao raio de nascimento (+35 cm de tolerância de centro de célula).
- Uma nova rota só é calculada quando o movimento direto está obstruído, ou quando o alvo muda substancialmente. Falhas recebem 0,7 s de espera entre tentativas.
- Cada monstro guarda a última posição vista do jogador por até 1,6 s para lidar com quinas; sem visão inicial, não começa a perseguição atrás de paredes.
- Reutiliza rotas no retorno ao ponto inicial, na fuga e no passeio aleatório quando houver obstáculos.
- Movimento amostrado a cada 12 cm no máximo, bloqueando atravessar canto diagonal.
- Alvos atravessando área segura, ataques à distância, alcance e dano continuam regulados pela lógica 05-M.
- Nenhuma nova dependência ou conexão de rede.

## Limitações

- Não é uma migração completa de `ai.lua`: patrulha roteirizada, IA de grupos e estado persistente fora da área ativa ainda não estão implementados.
- Não faz cálculo de colisão com volume real de cada monstro, apenas com a grade histórica do mapa.
- O caminho deve ser validado visualmente no Linux, especialmente ruas estreitas, portas e cantos em Argent City.
- Um alvo inicialmente fora da linha de visão não pode ser adquirido; depois de perda de visão, o monstro só persegue a última posição lembrada por um breve intervalo.

## Testes

```bash
cmake -S sources -B build-ui-tests -DTOP_BUILD_UI_TESTS=ON
cmake --build build-ui-tests -j4
ctest --test-dir build-ui-tests --output-on-failure
```

Nova suíte: `ui05n_monster_pathfinding`.

```bash
cmake -S sources -B build
cmake --build build -j4
./build/top_gl --check
./build/top_gl
```

Identificador: `top_gl offline - migracao 05-N (rotas A* locais de monstros; bolsa 24-48)`.
