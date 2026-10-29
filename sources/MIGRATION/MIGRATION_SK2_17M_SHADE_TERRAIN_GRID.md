# SK2.17-M — projeção de SHADE sobre o terreno

## Lacuna medida

O audit atual do Client encontrou 8 sistemas `type=13 (SHADE)` em 8 arquivos `.par`, alcançando 16 skills. Antes desta etapa, o ranking os marcava `missing_specialization`; o código GL ignorava o modelo aninhado de SHADE, mas desenhava o sistema pelo caminho genérico de quad. A implementação mudou a classificação para `partial`, que continua no ranking por causa da diferença de altura/obstáculo e da falta de validação visual. O inventário atual decodificou 1.031 `.par`, 1.227 `.eff` e 3.476 sistemas, sem falhas de decodificação (`build/sk217m-fx-coverage/`).

## Original DX9 x porte GL x mudança

| Original DX9 (fato) | Porte GL anterior (fato) | Mudança desta etapa |
|---|---|---|
| `PARTTICLE_SHADE` seleciona `_FrameMoveShade` e liga `m_bShade` (`arquivo/fontes/Engine/src/MPParticleSys.cpp:403-405`). O frame move usa uma partícula, fixa sua posição em `_vPos`, atualiza `CMPShadeCtrl::MoveTo` e encerra quando a timeline acaba (`MPParticleSys.cpp:2394-2425`). | O resolver já evitava interpretar `modelName` como geometria para SHADE (`sources/Engine/src/top/EffectSystem.cpp:605-614`), mas o renderizador colocava o tipo 13 no `quadRotated` genérico (`EffectSystem.cpp:2165-2200` na base anterior). | Com `TOP_HISTORICAL_SHADE=1`, o tipo 13 gera a grade projetada e usa a textura, filtro e par de blend do próprio `.par`. Desligado, mantém o comportamento anterior. |
| `CMPShadeMap::CreateShadeMap` usa o tamanho autorado como raio, `TILESIZE=1`, calcula `_fGridMax=radius+1` e arredonda `_iGridCrossNum` para cima (`arquivo/fontes/Engine/src/MPShadeMap.cpp:12,131-143`). `MoveTo` posiciona a grade, amostra terreno e monta UV centradas/clampadas (`MPShadeMap.cpp:315-423`). | Não havia grade de vértices de SHADE no porte. | `HistoricalShadeMesh.h` reproduz a grade, o truncamento do canto inicial, UV e bias `+0.015`; a altura vem de `MapFile::heightAt`. O limite de segurança deriva de `MAX_SHADER_VERNUM=300` e do array do SHADE original (`arquivo/fontes/Engine/include/I_Effect.h:251-252`; `MPShadeMap.h:82-85`). |
| O DX9 desenha `CMPShadeMap`, aplica cor via `D3DRS_TEXTUREFACTOR`/`D3DTA_TFACTOR`, blend autorado e desliga Z-write (`arquivo/fontes/Engine/src/MPParticleSys.cpp:1290-1297`; `MPShadeMap.cpp:546-588`). | O caminho GL já multiplica textura pela cor dos vértices; batches de efeito passam pelo mapeador D3D blend e pela política GL de depth write (`sources/Engine/src/gl/GfxDeviceGL.cpp:455-472,515-555`). | A cor interpolada da partícula vai nos vértices; o par de blend é aplicado pelo lote existente. O plano DX9 XY com Z-up é convertido para XZ com Y-up do GL. A chamada DX9 usa `D3DCULL_CCW`; o renderer GL atualmente desabilita culling (`GfxDeviceGL.cpp:348`), então os triângulos não dependem de winding para aparecer. |

## Fato, inferência e desconhecido

- **Fato:** o DX9 compõe cada vértice com `GetGridHeight`, `GetTileHeight` e `IsGridBlock`; usa o maior valor permitido e soma `0.015` (`MPShadeMap.cpp:389-415`).
- **Fato:** o porte GL compõe a grade usando `MapFile::heightAt` e soma `0.015` (`HistoricalShadeMesh.h:44-52`).
- **Inferência:** a grade preserva a forma e o recorte da textura para terrenos representados pelo mapa GL. Os 8 sistemas auditados comprovam que o formato é usado no Client; o contador de 16 skills é alcance estático do audit, não prova de execução numa sessão.
- **Desconhecido/limite:** `MapFile::heightAt` não fornece separadamente altura de objetos nem `IsGridBlock`. Por isso a projeção em obstáculos e bordas bloqueadas não equivale ao ramo DX9. Não houve comparação visual DX9/GL; SHADE continua **aproximado, não validado**.

## Testes executados

O fixture unitário verifica contagem de triângulos, UV, bias de altura, fallback numérico e estado do opt-in. A integração confirma que o GL anterior continua ativo sem a variável e que o caminho opt-in substitui quatro quads por uma grade 3×3 no fixture. Isso valida a montagem das malhas, não a aparência contra o DX9.

```bash
cmake -S sources -B build -DCMAKE_BUILD_TYPE=Debug -DTOP_BUILD_UI_TESTS=ON
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure

c++ -std=c++17 -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer \
  -Isources/Engine/include sources/tests/SK217MShadeMeshTests.cpp \
  -o build/sk217m_shade_mesh_asan
ASAN_OPTIONS=detect_leaks=0 ./build/sk217m_shade_mesh_asan

c++ -std=c++17 -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer \
  -Isources/Engine/include sources/tests/SK217MShadeIntegrationTests.cpp \
  sources/Engine/src/top/EffectSystem.cpp sources/Engine/src/top/TexturePaths.cpp \
  sources/Engine/src/top/ModelFile.cpp sources/Engine/src/top/MapFile.cpp \
  -lz -o build/sk217m_shade_integration_asan
ASAN_OPTIONS=detect_leaks=0 ./build/sk217m_shade_integration_asan

build/top_fx_coverage_audit --client Client --out build/sk217m-fx-coverage \
  --deps sources/auditoria-historica/dependencies.csv \
  --table sources/auditoria-historica/fx_port_coverage.csv \
  --source sources/Engine/src/top/EffectSystem.cpp
build/top_par_audit --client Client --out build/sk217m-par-audit
```

Executado em Debug: `top_gl` e os dois alvos SHADE compilaram; CTest passou **109/109**; os dois fixtures passaram com ASan/UBSan (`detect_leaks=0`). Auditorias atuais: 1.031 `.par`, 1.227 `.eff`, 3.476 sistemas, zero falhas de decode, zero drift e zero trilhas CMPStrip sem correspondência. O contador de `.eff` recusados é zero; portanto não existe `.eff` recusado atual a classificar como recurso não usado. O FX audit agora classifica SHADE como `partial` e mantém o tipo na lista de lacunas; não afirma que a implementação visual está completa.

## Comparação visual

No Client real, comparar a mesma skill no DX9 e GL, em terreno inclinado e junto a um obstáculo, alternando:

```bash
TOP_HISTORICAL_SHADE=0 ./build/top_gl --profile
TOP_HISTORICAL_SHADE=1 ./build/top_gl --profile
```

Desligue omitindo a variável ou usando `TOP_HISTORICAL_SHADE=0`. Para reverter o patch: `git apply -R SK217M.patch`.
