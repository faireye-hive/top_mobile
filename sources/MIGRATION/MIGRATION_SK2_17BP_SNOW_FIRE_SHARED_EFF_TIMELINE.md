# SK2.17-BP — relógio compartilhado de `.eff` filho em SNOW/FIRE

## Resumo

O DX9 inicia um `CMPModelEff` compartilhado quando `mediay=false` e avança esse filho uma vez por atualização do sistema SNOW/FIRE. O OpenGL anterior calculava o quadro do filho pela idade individual usada para desenhar cada partícula. Acrescentei uma timeline compartilhada opcional por sistema e passei a preservar/auditar o byte `mediay` da versão 15.

O comportamento novo exige `TOP_HISTORICAL_SNOW_FIRE_EFF_TIMELINE=1` e fica desligado por padrão. A etapa não afirma igualdade visual.

## Original DX9 × porte GL × mudança

| Original DX9 | Porte GL antes da etapa | Mudança |
|---|---|---|
| O arquivo grava e lê `_bMediay` em PAR v15 (`arquivo/fontes/Engine/src/MPParticleSys.cpp:4173,4333-4334`). O default do objeto é `false` (`MPParticleSys.cpp:99`). | O parser runtime descartava o byte final (`sources/Engine/src/top/EffectSystem.cpp:235`, antes desta etapa); o auditor também o ignorava (`sources/Engine/include/top/LegacyParAudit.h:143-144`, antes desta etapa). | Runtime e auditor preservam o byte somente para versão >14; versões anteriores continuam com `false` (`EffectSystem.cpp:236-239`; `LegacyParAudit.h:144-147`). |
| Com `mediay=false`, `SetModelName` cria um único `CMPModelEff` e chama `Play(0)` (`MPParticleSys.cpp:751-756`); `CMPPartSys::Play` reinicia o controlador compartilhado (`MPParticleSys.cpp:1110-1116`). | A renderização obtinha `modelAge` da idade da partícula e chamava `legacy_eff::framePhase(part, modelAge)` (`EffectSystem.cpp:2589-2596`, branch anterior). | `historical_eff_timeline::Group` avança uma fase comum do filho por perfil a cada `EffectSystem::step(dt)` e usa essa fase para todas as partículas desse sistema quando o switch está ligado (`EffectSystem.cpp:1328-1332,2541-2554`). |
| `_FrameMoveFire` e `_FrameMoveSnow` chamam `FrameMove(dwDailTime)` uma vez para o filho compartilhado (`MPParticleSys.cpp:3568-3576,3741-3749`). `CMPModelEff::FrameMove` soma o `dt`, avança no máximo um quadro por chamada e zera o residual (`arquivo/fontes/Engine/src/MPModelEff.cpp:500-538`). | A fase analítica era derivada da idade, podendo atravessar várias chaves em uma atualização. | A timeline reutiliza o controlador por quadro já usado em outros tipos. `Group::advance` descarta o residual ao avançar uma chave (`HistoricalEffTimeline.h:25-34,54-78`). A conversão é temporal; não copia estados D3D para GL nem altera blend, culling, winding ou matrizes. |
| Com `mediay=true`, o original cria um controlador por slot, reinicia o controlador no nascimento e atualiza todos (`MPParticleSys.cpp:741-749,3512-3518,3570-3574,3722-3728,3743-3746`). | O OpenGL não tinha esse modo por slot. | Esta etapa não implementa o modo por slot. O audit encontrou zero sistemas atuais com `mediay=true`; portanto a mudança cobre o caso compartilhado medido. Um Client com esse byte ligado exige uma etapa própria antes de afirmar suporte completo. |

## Auditoria dos dados

`top_par_audit --client Client --out build/sk217bp-mediay-audit` decodificou **1.031/1.031 PAR**, com **3.476 sistemas** e zero falhas de PAR. A distribuição de sistemas `mediay_eff=1` foi zero no arquivo todo. Entre tipos SNOW/FIRE, foram encontrados **566 SNOW** (367 PARs), **504 FIRE** (195 PARs), e **238 sistemas com modelo `.eff` filho**: 102 SNOW em 53 PARs e 136 FIRE em 50 PARs. Esses sistemas aparecem em 99 PARs distintos.

O FX audit atualizado inclui `feature:snow_fire_shared_nested_eff_timeline`: 238 sistemas, 99 PARs e 70 IDs estáticos alcançados no grafo. Esses vínculos são evidência de recursos/tabelas, não prova de execução em jogo. A execução corrente do FX audit também reportou 1.031 PAR, 1.227 EFF, 3.476 sistemas, 2.832 partes EFF, 25 lacunas agregadas, 307 IDs estáticos com lacuna, zero drift e zero falhas estruturais.

## Fato, inferência e desconhecido

- **FATO:** o DX9 tem um relógio `CMPModelEff` compartilhado para `mediay=false` e chama o update do filho dentro do update de SNOW/FIRE. O GL anterior amostrava o filho usando idade analítica por partícula. Os arquivos atuais têm 238 referências SNOW/FIRE a `.eff` e nenhum `mediay=true`.
- **INFERÊNCIA:** para esses 238 registros, uma fase compartilhada alimentada por cada `step(dt)` representa melhor a topologia temporal do DX9 do que reiniciar a fase pela idade de cada partícula.
- **DESCONHECIDO:** início exato do relógio em clientes com carregamento/preload diferente, sequência global de `rand()`, diferenças de `dt` em runtime, modo `mediay=true` em outros clientes e aparência comparada quadro a quadro. Não houve comparação visual DX9/GL.
- **EFF strict-only:** as 31 rejeições strict continuam classificadas pelo inventário SK2.17-BN; este passo não descarta nem muda decoder desses arquivos.

## Testes executados

- Configuração Debug: `cmake -S sources -B build -DCMAKE_BUILD_TYPE=Debug -DTOP_BUILD_UI_TESTS=ON` — exit code 0.
- Build completo incluindo `top_gl`: `cmake --build build -j$(nproc)` — exit code 0.
- CTest completo: `ctest --test-dir build --output-on-failure` — **140/140 aprovados**.
- Auditoria PAR atual: 1.031 decodificados, 0 falhas estruturais de PAR.
- FX audit atual: 25 lacunas, 307 IDs estáticos com lacuna, drift 0, falhas estruturais 0; a feature aparece em `fx_feature_coverage.csv` com 238 sistemas/99 arquivos/70 IDs.
- ASan/UBSan em build Debug separado, com `ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1`: teste integrado SNOW/FIRE, fixture PAR e auditoria FX passaram **3/3**.

## Ativação, reversão e comparação visual

Comparação no porte, com processo reiniciado:

```bash
TOP_HISTORICAL_SNOW_FIRE_EFF_TIMELINE=0 ./build/top_gl --profile
TOP_HISTORICAL_SNOW_FIRE_EFF_TIMELINE=1 ./build/top_gl --profile
```

Para reverter o patch completo: `git apply -R SK217BP.patch`. O teste integrado usa `.par` v15 e `.eff` sintéticos; ele confirma que o caminho compartilhado muda a fase e que o byte `mediay=true` não entra nesse caminho. Isso não valida visualmente o jogo real.

Checklist DX9 × GL: compare uma habilidade SNOW e uma FIRE com `.eff` filho, usando mesma câmera/posição e sequência de frames; observe filho compartilhado entre partículas, avanço após um `dt` maior que a chave, loop do conjunto e término quando o sistema encerra. Mantenha as demais variáveis idênticas e confira que Frozen Arrow, Illusion Slash, Taunt e Berserk não mudaram.
