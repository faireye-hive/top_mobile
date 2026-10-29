# Tales of Pirates — Migração 05-X2

## Problema observado no Linux

Após corrigir o desaparecimento do terreno na 05-X1, os testes do usuário mostraram aproximadamente 21–24 FPS na parte movimentada de Argent, 30–36 FPS em outro setor, picos de 42–48 FPS em regiões menos carregadas e pequenas quedas durante a troca da população. A RAM RSS ficou em aproximadamente 205–225 MiB. Isso não prova falta de RAM. Os 33 testes da 05-X1 passaram e o terreno voltou a aparecer corretamente.

## Diagnóstico no código

`GfxDevice::draw()` usava `glBufferData(GL_ARRAY_BUFFER, ..., GL_DYNAMIC_DRAW)` **para todos os batches, em todos os quadros**, inclusive terreno estático e partes da cena cujos vértices não mudaram. Isso transfere dados repetidamente e gera trabalho no driver. Uma segunda fonte potencial de lentidão é a reconstrução periódica de todo o cenário para animar uma pequena quantidade de objetos, ainda não eliminada nesta etapa.

## Alterações

1. **Buffers persistentes de GPU para o terreno**: `StaticBatchCache` mantém os identificadores VBO dos batches de `TerrainMesh`. A malha só é enviada no evento `WorldStreamSchedule::Update::terrain`, quando uma nova região precisa ser gerada.
2. **Buffers reutilizáveis de GPU para o cenário**: `StaticBatchCache` reenvia vértices dos `SceneBatch` somente quando `WorldStreamSchedule::Update::scene` pede atualização (atualmente até 15 Hz), sem alocar/destruir VBOs a cada quadro.
3. **Atualização de mapa limpa os caches**: ao viajar por portais, os VBOs antigos são liberados, impedindo renderizar o chão de um mapa anterior.
4. **Desenho de buffer estático**: `GfxDevice::drawStaticMesh()` reutiliza o VBO e preserva a mesma ordem de desenho, texturas, estados de transparência, shaders, iluminação, atlas e água. A animação da água troca somente a textura, não os vértices.
5. **Comparação A/B**: `--no-gpu-cache` faz terreno e cenário usarem novamente o caminho dinâmico, sem alterar recursos, distâncias, IA ou câmera.
6. **Perfil CPU aproximado**: `--profile` habilita o FPS/RAM e imprime médias por quadro das etapas de atualização do mundo (inclui simulação), reconstrução dos atores, chamadas de desenho e espera de troca de buffers (`swap`). Reporta número de draw calls e MiB enviados via `glBufferData`. A medição **não equivale ao tempo interno de GPU**, que exige consultas/timestamps de driver; `swap` pode incluir VSync.

## Testar no Linux

```bash
rm -rf build build-ui-tests
cmake -S sources -B build
cmake --build build -j4
./build/top_gl --check
./build/top_gl --profile
```

Compare depois, na **mesma área e resolução**, com:

```bash
./build/top_gl --profile --no-gpu-cache
```

Observe especialmente `profile medio`, `upload dinamico`, `cache`, `atores`, `render`, `swap`, `FPS` e a estabilidade das texturas ao atravessar 3 ou 4 setores e teleportar.

## Testes automatizados

```bash
cmake -S sources -B build-ui-tests -DTOP_BUILD_UI_TESTS=ON
cmake --build build-ui-tests -j4
ctest --test-dir build-ui-tests --output-on-failure
```

O novo teste é `ui05x2_gpu_cache_and_terrain_lifecycle`. Verifica que handles são reutilizados em atualizações, que frames sem alteração não reenviam malhas, que um teleporte libera os handles antigos e que a política de streaming do terreno continua solicitando reconstrução ao se deslocar.

Regressão local: **34/34 testes aprovados**. Nesta execução os 33 testes anteriores reaproveitaram executáveis já validados da 05-X1, enquanto o novo teste foi recompilado. `main.cpp` e `GfxDeviceGL.cpp` passaram em verificações de sintaxe C++17; no caso de `GfxDeviceGL.cpp`, foi usado um cabeçalho mínimo provisório somente para checagem de C++, pois este ambiente não dispõe de `GL/gl.h`. `UI05X2Tests` passou com AddressSanitizer e UBSan.

**Limitações**: ainda falta compilar e testar o executável OpenGL real no Linux do usuário. O ganho de FPS ainda **não foi medido em hardware**. A atualização de toda a cena a cada ciclo de animação, CPU skinning dos personagens e overhead das muitas chamadas de desenho continuam como possíveis gargalos que o novo perfil ajudará a localizar. Não mexemos na interface, física, scripts, saves, câmera ou seleção desta vez.
