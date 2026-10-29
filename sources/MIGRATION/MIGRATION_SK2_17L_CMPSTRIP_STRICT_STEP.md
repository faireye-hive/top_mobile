# SK2.17-L — limite estrito de amostragem CMPStrip

## Lacuna medida

O inventário do Client contém seis trilhas CMPStrip diretas. `01000012.par` tem `step=0.050000000745`; as outras cinco têm `step≈0.03` (`sources/auditoria-historica/DIRECT_CONTROLLER_STRIPS_SK212.csv`). O ponto de divergência é a comparação no limite: quando o tempo decorrido chega exatamente a `step`, o DX9 ainda não amostra.

## Original DX9 x porte GL x mudança

| Original DX9 (fato) | Porte GL antes desta etapa (fato) | Mudança desta etapa |
|---|---|---|
| `CMPStrip::FrameMove` soma `dt`, amostra apenas se `_fCurTime > _fStep` e zera o timer após `GetTrack` (`arquivo/fontes/Engine/src/MPModelEff.cpp:1292-1311`). | `legacy_direct_strip::append` rejeitava apenas quando `elapsed + epsilon < step`; igualdade passava (`sources/Engine/include/top/LegacyDirectStrip.h:72-80` na base SK2.17-K). | Com `TOP_HISTORICAL_CMPSTRIP_STRICT_STEP=1`, igualdade não amostra (`LegacyDirectStrip.h:75-88`). O comportamento anterior fica preservado se a variável estiver desligada ou ausente. |
| `GetTrack` acrescenta dois endpoints reais do item ou personagem; DX9 armazena translação nas linhas `_41.._43` (`arquivo/fontes/Engine/include/MPModelEff.h:814-860`). | O GL exige os dois dummies e converte D3D row-vector XYZ/Z-up para world X/height/Z ao extrair endpoint (`sources/Engine/include/top/LegacyDirectStrip.h:73-77`; origem capturada em `EffectSystem.cpp:1331-1348`). | Nenhuma coordenada, geometria, textura ou blend foi alterado. Só o instante de amostragem no limite foi isolado. |

## Fato, inferência e desconhecido

- **Fato:** a condição DX9 é estrita `>` e o timer reinicia em zero, não subtrai `_fStep` (`MPModelEff.cpp:1296-1310`). O GL anterior aceitava a igualdade; a nova opção corresponde a não aceitar esse instante.
- **Inferência:** a diferença aparece quando a soma dos `dt` cai exatamente no valor serializado. O inventário contém um `step≈0.05`, mas não se afirma que uma sessão real atinja esse limite em todos os FPS.
- **Desconhecido:** a equivalência em cadence variável depende dos frames efetivamente renderizados e das matrizes entregues. Sem captura DX9/GL, a trilha permanece não validada visualmente.

## Teste e ativação

O teste fixture confirma ambos os lados do limite: o caminho GL anterior ainda aceita `elapsed==step`; com o switch histórico, rejeita igualdade e aceita `elapsed>step`. A regressão de integração `ui05sk215_direct_controller_cmpstrip` cobre leitura do trailer, pares de dummies, UV, alpha e montagem da malha.

```bash
cmake --build build --target top_ui05sk215_tests top_gl -j$(nproc)
ctest --test-dir build -R 'ui05sk215_direct_controller_cmpstrip' --output-on-failure
c++ -std=c++17 -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer \
  -Isources/Engine/include sources/tests/SK215DirectStripTests.cpp \
  sources/Engine/src/top/EffectSystem.cpp sources/Engine/src/top/ModelFile.cpp \
  sources/Engine/src/top/TexturePaths.cpp sources/Engine/src/top/MapFile.cpp \
  -lz -o build/sk217l_cmpstrip_asan
ASAN_OPTIONS=detect_leaks=0 ./build/sk217l_cmpstrip_asan
ctest --test-dir build --output-on-failure
```

Executado em Debug: build completo e `top_gl` aprovados; CTest **107/107**; integração CMPStrip dentro do CTest aprovada; fixture CMPStrip + type-6 nested passou com ASan/UBSan (`detect_leaks=0`). O audit Client terminou com `par=1031`, `eff=1227`, `systems=3476`, `eff_parts=2832`, `direct_strip_records=6`, `unmatched_direct_strip_records=0`, `drift=0`, `failures=0`. O ranking mantém `direct_cmpstrip` como parcial, em 6 registros/16 skills.

No Client, habilite somente para a comparação A/B:

```bash
TOP_HISTORICAL_CMPSTRIP_STRICT_STEP=1 \
TOP_TRACE_DIRECT_STRIP=1 \
./build/top_gl --profile
```

Desligue a opção com `TOP_HISTORICAL_CMPSTRIP_STRICT_STEP=0`. Reverter a etapa: `git apply -R SK217L.patch`.
