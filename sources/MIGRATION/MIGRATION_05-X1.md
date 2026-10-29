# Migração 05-X1 — Correção de terreno desaparecendo durante a caminhada

Base: **05-X**. Compatível com o modo offline C++17/OpenGL.

## Defeito reproduzido

Na 05-X, `Platform/linux/main.cpp` só chamava `buildTerrainMesh()` quando
`movedSector` era verdadeiro, mas `meshX/meshY` eram sobrescritos também no
bloco de atualização de objetos animados da cena. Em mapas com vegetação
animada, o bloco executava várias vezes por segundo e acompanhava a posição
do jogador *sem reconstruir* o terreno. Consequência: o personagem saía da
janela antiga de tiles e aparecia a cor azul do clear buffer ao fundo, enquanto
os objetos 3D continuavam visíveis.

## Correção

- `WorldStreamSchedule` mantém a âncora da malha de terreno separada do tick
  de animação da cena.
- A âncora avança **somente** ao solicitar reconstrução da malha de terreno.
- Objetos animados continuam atualizando sua própria malha a aproximadamente
  15 Hz, mas não alteram a âncora do chão.
- Mudanças de mapa invalidam explicitamente a malha e seu agendamento.
- Teste independente cobre a sequência de mais de 30 metros de caminhada,
  animação contínua, deslocamento em Y e teleporte.

## Testes

`ui05x1_terrain_streaming_regression` confirma que as animações da cena não
cancelam a necessidade de atualizar o terreno. O resultado visual ainda deve
ser confirmado no Linux do jogador, andando vários setores.

## Desempenho

Nos logs 05-X fornecidos pelo jogador foram medidos ~22–43 FPS (cerca de
23–45 ms/quadro) e 206–226 MiB de RSS. Isso NÃO indica falta de RAM;
GPU/driver, triangulação, draw calls e atualização de atores podem contribuir.
O medidor `--perf`/Alt+P permanece disponível. A correção não promete melhora
de FPS: ela evita o desaparecimento do terreno, mantendo a otimização de não
reconstruí-lo só porque uma vegetação animou.

## Executar

```bash
cmake -S sources -B build
cmake --build build -j4
./build/top_gl --check
./build/top_gl --perf
cmake -S sources -B build-ui-tests -DTOP_BUILD_UI_TESTS=ON
cmake --build build-ui-tests -j4
ctest --test-dir build-ui-tests --output-on-failure
```
