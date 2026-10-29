# SK2.17-AM — relógio de atualização e filhos EFF do ROUND

## Resumo

O FX audit mede 42 sistemas ROUND (tipo 9) em 20 arquivos `.par`, ligados estaticamente a 7 skills. Dez desses sistemas, em 8 `.par` ligados a 4 skills, referenciam um filho `.eff`. Esta etapa adiciona uma timeline experimental por perfil: o ângulo compartilhado do grupo avança pelo `dt` de cada `EffectSystem::step`, cada partícula avança no máximo um quadro de curva por chamada, e o filho `.eff` avança no mesmo passo.

O modo exige `TOP_HISTORICAL_PARTICLE_EXTENDED=1` e `TOP_HISTORICAL_ROUND_TIMELINE=1`; ambos ficam desligados por padrão. A geometria e o mapeamento de eixos continuam no renderer OpenGL.

## Original DX9 x porte OpenGL x mudança

| Comportamento | DX9 original | Porte antes desta etapa | Mudança |
|---|---|---|---|
| Nascimento e órbita | `_CreateRound` cria a roda com `2π*n/count`, raio `range.y`, vida e cursor de curva (`arquivo/fontes/Engine/src/MPParticleSys.cpp:2882-2915`). `_FrameMoveRound` acumula `_fVecl*dt` no ângulo comum e subtrai uma volta uma vez quando `>= 6.283185` (`:2931-2934`). | `HistoricalParticleExtended::at` calcula a órbita por `speed*seconds`, sem estado por chamada (`sources/Engine/include/top/HistoricalParticleExtended.h:82-89`). | `SystemTimeline` mantém o ângulo do grupo e atualiza conforme cada `step(dt)` (`sources/Engine/include/top/HistoricalRoundTimeline.h:26-61`; `EffectSystem.cpp:1190-1204`). |
| Curva ROUND | `CMPParticle::GetCurFrame` soma `dt`, avança no máximo um quadro e zera o resíduo (`arquivo/fontes/Engine/include/MPParticleSys.h:116-129`). Quando chega ao quadro final, `_FrameMoveRound` reinicia o cursor e dá `continue`, preservando posição/tamanho/cor no update do wrap (`MPParticleSys.cpp:2941-2948`). | A amostra estendida usava `fmod(age,life)` e curva contínua calculada a partir da idade total (`HistoricalParticleExtended.h:73-75`; `EffectSystem.cpp:2168-2176`). | A timeline usa o cursor discreto existente de MODEL, retém uma amostra no wrap e fornece fase por slot ao renderer (`HistoricalRoundTimeline.h:55-79`; `EffectSystem.cpp:2078-2085,2177-2182`). |
| Filho `.eff` | `_FrameMoveRound` chama `CMPModelEff::FrameMove(dwDailTime)` antes de atualizar as partículas (`MPParticleSys.cpp:2926-2929`). `CMPModelEff::FrameMove` avança no máximo um quadro e ROUND inicia o filho por `Play(0)` (`arquivo/fontes/Engine/src/MPModelEff.cpp:500-540`; `MPParticleSys.cpp:982-997`). | O relógio do filho ROUND não era armazenado; a geometria era amostrada analiticamente pelo tempo da instância. | Um `historical_eff_timeline::Group` por perfil avança uma vez por `step(dt)` e é usado por quadros/UVs quando o switch ROUND está ativo (`EffectSystem.h:223`; `EffectSystem.cpp:1105-1108,1200-1202,2374-2393`). |
| Convenção de coordenadas | DX9 usa X/Y no plano e Z vertical; `D3DXMatrixRotationZ` gira `(0,range.y,0)` no plano XY (`MPParticleSys.cpp:2884-2894,2960-2965`). | GL renderiza em Y-up e seus vetores ficam em X/Z. | A timeline conserva posições no espaço DX9; a fronteira do renderer mapeia `DX9 X/Y/Z` para `GL x/z/y` (`EffectSystem.cpp:2265-2270`). Não copia matrizes D3D nem altera winding/culling. |

## Fato, inferência e desconhecido

- **Fato:** o código DX9 atualiza um ângulo comum uma vez por `FrameMove`; cada cursor `GetCurFrame` descarta o resíduo quando cruza um limite e avança uma única vez.
- **Fato:** o audit atual conta 42 ROUND em 20 `.par`/7 skills e 10 referências `.eff` em 8 `.par`/4 skills. A referência serializada comprova vínculo estático, não execução durante uma sessão.
- **Fato:** `TOP_HISTORICAL_ROUND_TIMELINE` fica desligado por padrão e só é aplicado junto com `TOP_HISTORICAL_PARTICLE_EXTENDED`.
- **Inferência:** o relógio por chamada reproduz melhor a sequência do controlador DX9 do que amostrar `speed*age` e `fmod(age,life)`, especialmente quando um `dt` atravessa vários quadros.
- **Desconhecido:** a sequência global de `rand()` (incluindo a fase inicial `Rand(100)`), a cadência real do Client DX9, solicitações indiretas de recursos e a paridade visual. O audit não prova que todas as 7 skills foram executadas.
- **Limite:** ROUND permanece aproximado e não validado visualmente. Não foram alteradas outras habilidades nem escalas.

## Auditoria atual e `.eff` com falha estrita

Com `build/top_fx_coverage_audit --client Client --out build/sk217am-round-final --controller-strips sources/auditoria-historica/DIRECT_CONTROLLER_STRIPS_SK212.csv`, o audit leu 1.031 `.par`, 1.227 `.eff`, 3.476 sistemas e 2.832 partes EFF; resultado: 26 lacunas, `drift=0`, falhas estruturais `0`, `strict_eff_decode_failures=31`.

As 31 recusas estritas têm recuperação estrutural no modo diagnóstico: 30 são referenciadas diretamente por `.par`; `fboomerff.eff` não tem referência direta, mas `fboomerff.par` aparece associado à cena 919 e à skill 308. O uso em runtime não está provado. Portanto, esta etapa não classifica nenhum deles como inútil nem remove suporte. Os resultados individuais permanecem em `build/sk217am-round-final/fx_eff_usefulness.csv`.

## Testes e build executados

- `cmake -S sources -B build -DCMAKE_BUILD_TYPE=Debug -DTOP_BUILD_UI_TESTS=ON` — sucesso.
- `cmake --build build -j$(nproc)` — sucesso; inclui `top_gl`.
- `ctest --test-dir build --output-on-failure` — **135/135 passaram**.
- Os testes novos verificam quadro único por atualização, descarte do resíduo, wrap e retenção da última amostra, ângulo comum e mudança de curva no renderer integrado.
- Os executáveis unitário e integrado novos também passaram compilados com `-fsanitize=address,undefined` (`ASAN_OPTIONS=detect_leaks=0`).
- Não houve comparação visual com DX9. O CTest é CPU/headless e não valida o resultado OpenGL na tela.

## Checklist visual

Compare a mesma cena/skill ROUND com `TOP_HISTORICAL_ROUND_TIMELINE=0` e `=1`, mantendo `TOP_HISTORICAL_PARTICLE_EXTENDED=1`. Preserve os switches já usados para o Frozen Arrow:

```bash
TOP_HISTORICAL_BLEND=1 \
TOP_HISTORICAL_MODEL_TEXTURE=1 \
TOP_FROZEN_RESULT_GROUND=1 \
TOP_HISTORICAL_FILTERS=1 \
TOP_HISTORICAL_PARTICLE_POOL=1 \
TOP_HISTORICAL_PARTICLE_MOTION=1 \
TOP_HISTORICAL_PARTICLE_EXTENDED=1 \
TOP_HISTORICAL_ROUND_TIMELINE=1 \
./build/top_gl --profile
```

Repita com `TOP_HISTORICAL_ROUND_TIMELINE=0` para o fallback anterior. Reinicie `top_gl` ao trocar a variável. O switch é removido com `unset TOP_HISTORICAL_ROUND_TIMELINE`.

Patch relativo à raiz: `SK217AM.patch`. Confira/aplique com `git apply --check SK217AM.patch` e `git apply SK217AM.patch`; reverta somente esta etapa com `git apply -R SK217AM.patch`.
