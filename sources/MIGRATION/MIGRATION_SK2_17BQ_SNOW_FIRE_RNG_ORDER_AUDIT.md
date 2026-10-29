# SK2.17-BQ — auditoria da ordem de RNG em SNOW/FIRE

## Resultado

Auditoria comparativa, sem alteração de runtime. A ordem DX9 de sorteios de nascimento é recuperável nos callsites, mas o porte usa hashes determinísticos com sementes diferentes para movimento e vida e não compartilha o estado global. Mudar offsets/lane numbers sem unificar esse fluxo não demonstraria maior fidelidade; a tentativa de mudança foi retirada.

## DX9 original × porte OpenGL × decisão

| Comportamento | DX9 original | Porte GL atual | Decisão |
|---|---|---|---|
| `Rand`/`Randf` | `Rand(n)` consome `rand()%n`; `Randf(max)` pode consumir um ou dois draws conforme a parte inteira/fracionária; `Randf(a,b)` aplica `Randf(b-a)` e soma `a` (`arquivo/fontes/Engine/include/MPParticleSys.h:6-44`). | `dx9Randf` conserva módulo/quantização, porém recebe `deterministicRandBits(seed,lane)` (`sources/Engine/include/top/HistoricalParticleRandom.h:24-78`). | **FATO:** forma do mapeamento preservada sob `TOP_HISTORICAL_PARTICLE_RANDF=1`; sequência MSVC global não é. |
| FIRE no nascimento | `_CreateFire` normaliza a direção, sorteia posição nos eixos (ou um índice `Rand` de model-range), sorteia vida e depois `m_fPartTime` (`arquivo/fontes/Engine/src/MPParticleSys.cpp:3462-3504`). | A posição usa seed baseado em `0x85ebca6b`/`0xc2b2ae35` e lanes por eixo; vida usa seed baseado em `0x9e3779b9`/`0x85ebca6b` (`sources/Engine/include/top/HistoricalParticleMotion.h:48-54`; `HistoricalParticleRandom.h:81-87`). | Os números de lane podem coincidir sem gerar o mesmo draw, pois as sementes não coincidem. Não há evidência para chamar isso de correlação ou corrigir offsets. |
| SNOW no nascimento | Com divisor diferente de 1, `_CreateSnow` sorteia três magnitudes de velocidade, dois sinais, posição e vida; rejeita velocidade Z zero antes de sortear posição/vida (`arquivo/fontes/Engine/src/MPParticleSys.cpp:3681-3715`). `Rand(100)` define `m_fPartTime` depois (`:3720`). | GL tem lanes semânticos independentes para posição e velocidade e sinais; vida vem de outro seed por slot/geração (`sources/Engine/include/top/HistoricalParticleMotion.h:51-72`; `HistoricalParticleRandom.h:81-87`). | A distribuição/quantização pode ser traduzida; a sequência por slot não representa a ordem global do DX9. |
| Atualização/aceleração | `_FrameMoveFire` consome `rand()%2` por partícula viva que não terminou o quadro, em ordem de slots; slots mortos podem chamar `_CreateFire` entre esses draws (`arquivo/fontes/Engine/src/MPParticleSys.cpp:3579-3618`). SNOW segue a mesma estrutura de slots e sorteia um sinal por partícula atualizada (`:3752-3789`). | `SystemTimeline` visita slots por índice e deriva sinais determinísticos por slot/geração/passo; a emissão usa também contador por perfil (`sources/Engine/include/top/HistoricalParticlePool.h:54-118`). | A ordem de slots se aproxima, mas a mistura de draws vivos/nascimentos no mesmo estado global não é reproduzida. |

## Auditorias e dados atuais

- `top_par_audit` salvo em `build/sk217bp-mediay-audit/par_systems.csv`: 1.070 controladores SNOW/FIRE, sendo 566 SNOW e 504 FIRE; todos têm `model_range=0`. 763 têm `range_x` não zero e 222 têm os três eixos de range zerados. O conjunto atual não exige escolher um índice de model-range para nascer.
- Reexecução `python3 sources/Tools/particle_rng_audit.py --original-root arquivo/fontes --gl-root sources/Engine --out build/sk217bq-rng-order-audit` concluiu com exit 0: DX9 tem 41 sites `rand()` no Client, 13 na Engine e 9 em Libraries classificados como candidatos de runtime; há 8 `srand()` no Client e 2 em Libraries candidatos. O OpenGL não tem sites `rand()`/`srand()` e registra 27 referências lexicais a helpers determinísticos. São sites lexicais, não contagens executadas.
- O scanner existente declara seu limite: não prova alcance em runtime nem ordem de chamada (`sources/Tools/particle_rng_audit.py`, `collect_sites`/`audit`). A contagem de assets mede dados, não uso durante uma sessão.

## Fato, inferência e desconhecido

- **FATO:** os callsites DX9 citados definem ordem condicional de draws por criação, quadro e slot. No porte, o seed de movimento difere do seed de vida. A auditoria Client não encontrou SNOW/FIRE com model-range.
- **INFERÊNCIA:** replicar somente offsets de lane sem um único fluxo por nascimento teria efeito determinístico diferente, mas não uma justificativa demonstrável de fidelidade. Por isso a alteração exploratória foi removida.
- **DESCONHECIDO:** estado do `rand()` global em cada criação de efeito, interleaving com Client/Lua/EncLib, a sequência de draws em runtime e a aparência resultante comparada com DX9. O scanner lexical não resolve esses pontos.
- **Limite de dados:** outros Clients podem conter SNOW/FIRE com model-range; a conclusão de ausência aplica-se somente aos 1.070 registros auditados no Client atual.

## Testes e validação

- Nenhuma mudança de runtime permaneceu nesta etapa; os switches atuais continuam inalterados.
- `cmake -S sources -B build -DCMAKE_BUILD_TYPE=Debug -DTOP_BUILD_UI_TESTS=ON` — exit 0.
- `cmake --build build -j$(nproc)` após restaurar os arquivos de runtime ao snapshot anterior — exit 0; `top_gl` incluído.
- `ctest --test-dir build --output-on-failure` — exit 0, 140/140.
- O audit RNG terminou com exit 0. Não houve execução do jogo nem comparação visual DX9/OpenGL.

## Próxima investigação

Para alterar o fluxo global com evidência, falta um trace de runtime do DX9 que capture `srand`/`rand` (callstack e ordem por frame) para uma habilidade SNOW/FIRE, incluindo se há chamadas intercaladas de Client/Lua/EncLib. Sem isso, manter a amostragem determinística aproximada.
