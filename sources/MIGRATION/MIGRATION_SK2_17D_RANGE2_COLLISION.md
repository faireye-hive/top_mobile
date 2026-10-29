# SK2.17-D — RANGE2: colisão e efeito de impacto

## Resumo

Esta etapa implementa, atrás de `TOP_HISTORICAL_RANGE2_COLLISION=1`, o encerramento de RANGE2 por colisão e o disparo do recurso `.par` indicado por `hitEffect`. A opção também exige `TOP_HISTORICAL_PARTICLE_EXTENDED=1`; ambas ficam desligadas por padrão. O controlador usa passo nominal de 60 Hz, portanto é uma reprodução determinística aproximada do `dt` variável do DX9.

## Evidência DX9 e porte

| Original DX9 | Porte antes da SK2.17-D | Mudança desta etapa |
|---|---|---|
| `_CreateRange2` acumula `_fCurTime`, emite após `_fStep`, grava a direção normalizada e usa `range[0/1]` aleatório com `range[2]` fixo: `arquivo/fontes/Engine/src/MPParticleSys.cpp:2142-2193`. `Randf(max)` retorna valor no intervalo não negativo: `arquivo/fontes/Engine/include/MPParticleSys.h:26-38`. | O tipo 15 era processado como movimento estendido contínuo, sem estado de colisão ou callback: `sources/Engine/include/top/HistoricalParticleExtended.h:121-127`; o próprio ranking marcava colisão pendente. | `HistoricalRange2Collision::at` avalia nascimento e movimento determinísticos, testa terreno/limites e encerra a partícula: `sources/Engine/include/top/HistoricalRange2Collision.h:27-73`; `EffectSystem` usa essa amostra em `sources/Engine/src/top/EffectSystem.cpp:1736-1748,1799-1805`. |
| O DX9 move pela direção normalizada vezes `_fVecl * dt`, consulta `GetGridHeight((int)x*2,(int)y*2)` e colide quando `z < terreno` ou `z > 50`; com mapa, marca morta, ajusta Z para o terreno e chama `SendResMessage`: `arquivo/fontes/Engine/src/MPParticleSys.cpp:2218-2236`. Sem mapa, usa `z < 0 || z > 50`, mata e envia a posição sem ajustar Z: `:2238-2245`. | O leitor `.par` não preservava o nome histórico do efeito de impacto; consumia os 32 bytes sem uso. | `hitEffect` é lido em `sources/Engine/src/top/EffectSystem.cpp:214-216`; o nome resolve `.par` em `:572-594`; a instância de impacto é criada uma vez por slot em `:1197-1227`. A diferença entre clamp com mapa e sem mapa é mantida em `HistoricalRange2Collision.h:64-70`. |
| Depois de marcar o slot morto, o loop chama `_CreateRange2` novamente enquanto `_bStop` for falso: `arquivo/fontes/Engine/src/MPParticleSys.cpp:2256-2259`. | A aproximação anterior do tipo 15 não mantinha estado de colisão. | Esta entrega encerra o slot após a primeira colisão e não o recicla. Portanto, recupera o callback da colisão, mas o ciclo de pool continua parcial e não deve ser considerado o RANGE2 completo. |
| O nome `m_strHitEff` faz parte do formato: grava 32 bytes em `arquivo/fontes/Engine/src/MPParticleSys.cpp:4161-4163` e lê versões >10 em `:4305-4309`. | A renderização converte partículas DX9 de Z-up para mundo OpenGL X/altura/Y, mas o parser não emitia o impacto nomeado. | O impacto traduz `{DX9 X,Y,Z}` para `{OpenGL X,Z,Y}` ao criar a instância (`sources/Engine/src/top/EffectSystem.cpp:1203-1205`). Não há mudança de blend, culling ou winding nesta etapa. |

## Tradução de coordenadas e limites

O controlador conserva as coordenadas do DX9 (X/Y horizontais, Z vertical) até a borda do renderer. Essa é uma reordenação para o mundo OpenGL do porte, que usa X/altura/Y; não é cópia de estado Direct3D. O original usa `D3DXVec3Normalize` em `_CreateRange2` e reaplica a direção normalizada por velocidade e `dt`; o helper usa a norma Euclidiana e um relógio fixo nominal de 60 Hz. Isso preserva a regra vetorial, mas não o tempo variável exato por quadro.

**Limitação de terreno conhecida:** o DX9 amostra uma grade de meio tile e alturas de vértices por tile (`arquivo/fontes/Engine/include/MPMap.h:346-356`; preenchimento da grade em `arquivo/fontes/Engine/src/MPMap.cpp:1128-1136`). O porte só expõe `MapFile::heightAt`, que interpola altura entre tiles inteiros (`sources/Engine/include/top/MapFile.h:51-53`, `sources/Engine/src/top/MapFile.cpp:173-190`). O adaptador atual usa essa API; por isso a colisão com mapa é funcional, mas a amostra de terreno ainda é aproximada. A próxima lacuna para alegar paridade seria carregar/expor as quatro alturas de subgrade do `.map` no `MapFile`.

## Auditoria dos recursos e cobertura

Executado com `Client/` da raiz pelo `build/top_fx_coverage_audit` e `build/top_par_audit`:

- O inventário atual encontrou 1.031 `.par`, 1.227 `.eff`, 3.476 sistemas e 2.639 partes `.eff`; registrou 19 gaps de implementação, 307 skills com gaps, 31 falhas/arquivos `.eff` não totalmente decodificados e zero drift na tabela de cobertura usada pelo auditor.
- RANGE2 (tipo 15) aparece em 35 sistemas de `.par`; todos os 35 registros examinados têm `hitEffect` preenchido. Isso mede presença de recursos, não comprova que cada skill executa em jogo.
- **Implementados e testados em código:** BLAST2 tem teste de estados e integração desde a SK2.17-C; SK2.17-D cobre a colisão/bounds e emissão de impacto RANGE2 com testes unitário e integrado. Isso comprova caminhos sintéticos, não paridade visual.
- **Parciais/aproximados segundo o ranking:** tipos 1 (SNOW), 2 (FIRE) e 3 estão marcados aproximados; tipo 5 e 6 parciais; tipo 4 e 12 parciais; tipos 7–11 e 14–16 aproximados. O relatório específico por tipo e seus motivos está no CSV, sem converter essas categorias em “ausente”.
- **Especialização ausente no renderer:** tipo 13 (SHADE) está classificado `missing_specialization`; o código de resolução pula `type==13` em `sources/Engine/src/top/EffectSystem.cpp:601-603`, e o auditor identifica fallback genérico. As 8 instâncias são dados referenciados, não execução observada.
- **SNOW/FIRE:** o porte tem ciclo de pool e movimento com switches das etapas SK2.16 e aceleração aleatória determinística de 60 Hz; o ranking ainda marca cadência do emissor/RNG como aproximados. Isso não fecha o ciclo exato de emissão, reciclagem, aceleração e fim.
- **BLAST/BLAST2/BLAST3:** BLAST2 tem estados dedicados e testes desde SK2.17-C. O ranking mantém BLAST (tipo 9) e BLAST3 (tipo 11) como aproximados; 60 Hz e callback/quadros ainda limitam equivalência.
- **WIND, ROUND, ARRAW, RANGE, DUMMY, LINE_SINGLE e LINE_ROUND:** este ranking atual não classifica individualmente cada nome/controlador; portanto não afirmo ausência nem equivalência. Necessitam cruzamento de enum/dispatch DX9 com recursos antes de um status por mecanismo.
- **Incompleto/sem prova de execução:** `top_par_audit` decodificou 1.031/1.031 `.par`, com 110 referências ausentes; o FX audit registrou 31 `.eff` parcialmente decodificados. Nenhum dos dois comprova execução de todos os efeitos.
- **Limitação do auditor:** a execução pós-SK2.17-D continuou emitindo para tipo 15 a anotação antiga “terrain hit callback pending” (`build/sk217d-fx-coverage-post/fx_gap_ranking.csv`), embora o código e os testes agora cubram o callback. A tabela de status do auditor está desatualizada e precisa de tarefa própria para detectar a implementação no código; não use o rótulo do tipo 15 pós-etapa como verdade atual.

Os números acima são saídas da auditoria executada antes da mudança; o mesmo inventário foi repetido depois. Os CSVs foram gravados em `build/sk217d-fx-coverage/`, `build/sk217d-fx-coverage-post/`, `build/sk217d-par-audit/` e `build/sk217d-par-audit-post/`. A saída do PAR permaneceu 1.031 decodificados, zero falhas, 3.476 sistemas e 110 referências ausentes.

## Testes e estado de validação

- Fixture sintética `.par` versão 11 verifica leitura do nome de impacto e integração do efeito nomeado.
- Teste puro cobre nascimento, movimento, colisão com terreno, clamp com mapa, limites sem mapa sem clamp, vida do controlador e normalização da direção.
- Build Debug completo compilou `top_gl` e os alvos de teste.
- `ctest --test-dir build --output-on-failure`: 103/103 passou na repetição final. A primeira execução apresentou uma falha isolada do `ui05sk214_fx_coverage_audit`; executado isoladamente e na repetição completa, passou. Nenhum teste foi removido ou relaxado.
- Os dois testes SK2.17-D também passaram com AddressSanitizer e UndefinedBehaviorSanitizer, com `ASAN_OPTIONS=detect_leaks=0`. A execução padrão do LeakSanitizer falhou no ambiente com “does not work under ptrace”; portanto não foi possível usar LSan.
- Os testes não fazem comparação visual. A SK2.17-D é experimental e não deve ser chamada visualmente idêntica ao DX9.

## Como executar e reverter

Comparação em sessão Linux, mantendo as opções visuais anteriores:

```bash
TOP_HISTORICAL_BLEND=1 \
TOP_HISTORICAL_MODEL_TEXTURE=1 \
TOP_FROZEN_RESULT_GROUND=1 \
TOP_HISTORICAL_FILTERS=1 \
TOP_HISTORICAL_PARTICLE_POOL=1 \
TOP_HISTORICAL_PARTICLE_MOTION=1 \
TOP_HISTORICAL_PARTICLE_EXTENDED=1 \
TOP_HISTORICAL_RANGE2_COLLISION=1 \
./build/top_gl --profile
```

Para desligar apenas a SK2.17-D, use `TOP_HISTORICAL_RANGE2_COLLISION=0` ou remova essa variável. Para desfazer o patch completo após aplicá-lo numa cópia limpa da SK2.17-C: `git apply -R SK217D.patch`.

## Desconhecido / próximo passo

**FATO:** o DX9 usa `GetGridHeight` na subgrade, chama `SendResMessage` quando o slot cruza o chão/limite e tenta recriar o slot morto enquanto o emissor não parou. **INFERÊNCIA marcada:** o mapeamento atual por `MapFile::heightAt(int(x),int(y))` aproxima a altura de impacto disponível ao renderer. **DESCONHECIDO:** paridade de amostra por vértice, `dt` por frame, distribuição aleatória exata por execução, duração/percepção visual dos efeitos e comportamento em todas as skills. Próximo passo: completar a reciclagem após impacto e verificar novamente os 7 skills com RANGE2; a altura por subgrade permanece uma limitação registrada.

## Errata de auditoria — SK2.17-BC

A afirmação anterior de que o audit pós-SK2.17-D ainda emitia “terrain hit callback pending” foi corrigida pelo inventário por tipo. `fx_hit_effect_coverage.csv` vincula RANGE2 (15) ao `SendResMessage` do DX9 e ao caminho GL condicionado por `TOP_HISTORICAL_RANGE2_COLLISION`. Isso confirma caminho de callback e testes, não paridade visual ou execução de todas as skills. Ver `MIGRATION_SK2_17BC_HIT_EFFECT_DISPATCH_AUDIT.md`.
