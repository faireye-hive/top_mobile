# SK2.17-BF — auditoria do fluxo global de RNG e utilidade dos `.eff`

## Escopo medido

Esta etapa adiciona somente uma auditoria estática; não muda o renderer nem o comportamento do jogo. O scanner remove comentários e literais antes de localizar `rand()`/`srand()`, diferencia Client, Engine e Libraries, e marca declarações MaxSDK e código do Log Viewer separadamente. Ele não prova que uma chamada é alcançável em runtime nem reconstrói a ordem real dos draws.

Execução contra o DX9 em `arquivo/fontes` e o porte em `sources/Engine`:

- DX9: 41 sites `rand()` no Client, 13 na Engine, 9 em Libraries classificados como candidatos de runtime; 8 `srand()` no Client e 2 em Libraries (Lua e EncLib). O inventário também identificou 4 declarações MaxSDK e 2 sites da ferramenta Log Viewer, separados dos candidatos de runtime.
- OpenGL: nenhum `rand()`/`srand()` no Engine; 23 referências lexicais aos helpers determinísticos.
- Os números são sites de código, não quantidade de chamadas executadas.

## Original DX9 × porte OpenGL × decisão

| Original DX9 | Porte OpenGL atual | Resultado desta etapa |
|---|---|---|
| `Rand`, `Randf(float)` e `Randf(a,b)` consomem o `rand()` global e aplicam módulo/quantização (`arquivo/fontes/Engine/include/MPParticleSys.h:6-44`). FIRE e SNOW sorteiam valores na criação e sinais durante atualização (`MPParticleSys.cpp:3487-3504,3610-3618,3778-3789`). | `HistoricalParticleRandom.h` traduz o mapeamento e o módulo, mas injeta valores de um fluxo determinístico por partícula; o próprio código registra que não replica a ordem global (`sources/Engine/include/top/HistoricalParticleRandom.h:24-28,55-66`). Pool/movimento usam esse fluxo por slot/frame (`HistoricalParticlePool.h:56-58,90-101`; `HistoricalParticleMotion.h:37-40,50-70,90-99`). | Manter `TOP_HISTORICAL_PARTICLE_RANDF` opt-in. Não substituir pelo fluxo global: o audit encontrou múltiplos consumidores e reseeds independentes, sem uma sequência de execução completa comprovável. Nenhuma alteração de runtime nesta etapa. |
| O Client reseeda durante `WM_PAINT` e consome `rand()` para escolher textura de loading (`arquivo/fontes/Client/src/GameLoading.cpp:63-72`). UI, criação de personagem e seleção de servidor também têm seus próprios `srand` (`CreateChaScene.cpp:853`; `ServerSet.h:208-210`; UI: sites listados no CSV). | O subsistema de partículas GL não compartilha esse estado de UI. | **FATO:** esses callsites existem. **INFERÊNCIA:** a UI pode alterar o estado global antes de um efeito. **DESCONHECIDO:** se/ quando isso ocorre na sessão real e a ordem de todos os consumidores. |
| Lua `math.randomseed` chama `srand` e `math.random` chama `rand` (`arquivo/fontes/Libraries/CaLua/Lua/lib/lmathlib.c:170-199`). EncLib também chama `srand` em `SetEncKey` e consome quatro draws por `Encrypt` (`arquivo/fontes/Libraries/EncLib/src/enclib.cpp:59-63,84-89`). | Sem estado global compartilhado no Engine OpenGL auditado. | A auditoria não encontrou evidência suficiente para afirmar que esses caminhos rodam durante efeitos. Eles impedem tratar a sequência da Engine como isolada, mas sua contribuição por sessão continua **desconhecida**. |

### Conclusão sobre `.eff` que falharam em strict decode

A auditoria FX já salva em `build/sk217be-fx-audit/` lista 31 recusas do decoder estrito. As 31 têm `diagnostic_decode=decoded_with_raw_model_params`, zero constam em `fx_decode_failures.csv`, e todas têm evidência DX9 `enumerated_loaded_indexed`. Trinta têm referência direta de `.par`; a exceção `effect/fboomerff.eff` não tem referência direta, mas tem o irmão `fboomerff.par`, catálogo de cena 919/skill 308 e modelos/partes decodificados. Para esse caso, execução em runtime não está provada.

**Decisão:** não classificar nem remover nenhum dos 31 como inútil. Para 30, o vínculo direto com PAR comprova utilidade como dependência de dados; para `fboomerff.eff`, evidência de catálogo/carga impede descartá-lo, embora uso efetivo permaneça desconhecido. A recusa strict é causada pelos parâmetros-modelo não finitos; o decoder diagnóstico preserva os valores crus para inspeção. Ainda falta confirmar se cada parâmetro é consumido pelo renderer DX9 ou se algum deles é ignorado. A prioridade adequada é documentar/validar a semântica dos campos, não pular a decodificação.

## Artefatos e execução

- Scanner: `sources/Tools/particle_rng_audit.py`
- Fixture: `sources/tests/test_particle_rng_audit.py`
- Saídas da execução: `build/sk217bf-particle-rng-audit/particle_rng_sites.csv` e `particle_rng_summary.json`
- Fixture executada: `python3 sources/tests/test_particle_rng_audit.py` — 1/1 passou.
- Configuração Debug `cmake -S sources -B build -DCMAKE_BUILD_TYPE=Debug -DTOP_BUILD_UI_TESTS=ON` — exit 0.
- Build `cmake --build build -j$(nproc)` — exit 0, incluindo `top_gl`.
- CTest `ctest --test-dir build --output-on-failure` — 138/138 passaram.
- Auditoria executada: `python3 sources/Tools/particle_rng_audit.py --original-root arquivo/fontes --gl-root sources/Engine --out build/sk217bf-particle-rng-audit` — terminou com código 0.

Sem mudança C++ de runtime. O build Debug e os testes foram executados para regressão. Sem comparação visual. Não declarar fidelidade visual: a sequência global, a reachability por sessão e os efeitos `.eff` seguem sem validação visual DX9/OpenGL.
