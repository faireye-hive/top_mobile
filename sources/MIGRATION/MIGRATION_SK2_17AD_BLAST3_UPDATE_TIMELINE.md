# SK2.17-AD — timeline de atualização do BLAST3

## Resultado

O BLAST3 tinha apenas deslocamento radial analítico no porte. Esta etapa adiciona uma simulação por atualização, opcional, para o controlador pai type 11. Ela conserva a implementação anterior como default e exige `TOP_HISTORICAL_PARTICLE_EXTENDED=1` junto com `TOP_HISTORICAL_BLAST3_TIMELINE=1`.

## Evidência: original DX9 x porte GL x mudança

| Regra | Original DX9 | Porte antes desta etapa | Mudança |
|---|---|---|---|
| Emissão | `_CreateBlast3` distribui direção com ângulo `2π*n/count`, transforma `(1,1,-1)`, soma `_vDir` e inicia todos em `_vPos` (`arquivo/fontes/Engine/src/MPParticleSys.cpp:2577-2606`). | O caminho type 11 gerava posição radial por idade analítica (`sources/Engine/src/top/EffectSystem.cpp:2040-2049,2053-2060`). | Timeline inicializa direção/posição por partícula em `HistoricalBlast3Timeline.h:43-57`. |
| Convenção de matriz | `D3DXMatrixRotationYawPitchRoll` seguida de `D3DXVec3Transform` (`MPParticleSys.cpp:2588-2595`) opera sobre o vetor original. | GL usa coordenadas de mundo x/y/z próprias. | A rotação Z é traduzida para vetor-linha: `x=cos−sin`, `y=sin+cos`; o eixo vertical do espaço DX9 é remapeado para Y de mundo GL somente no limite existente (`EffectSystem.cpp:2107-2113`). |
| Relógio de quadro | `GetCurFrame` soma `dt`, avança no máximo um quadro e zera o residual (`arquivo/fontes/Engine/include/MPParticleSys.h:116-129`). BLAST3 encerra o sistema assim que o primeiro slot retorna `frameCount` (`MPParticleSys.cpp:2632-2637`). | A idade do pai seguia o tempo total, sem estado de quadro próprio. | `advance` mantém quadro/resíduo por slot, descarta excesso e interrompe o grupo no primeiro término (`HistoricalBlast3Timeline.h:72-85`). |
| Movimento | Atualiza `pos += m_vOldPos * (_fVecl*dt)` e, depois, `m_vOldPos += accel*dt` (`MPParticleSys.cpp:2645-2658`). | A posição usava uma aproximação radial genérica. | O módulo preserva a ordem movimento-antes-aceleração (`HistoricalBlast3Timeline.h:87-91`). |
| Filho `.eff` | `_FrameMoveBlast3` avança `_CPPart` antes do loop de partículas (`MPParticleSys.cpp:2623-2626`). | O fallback GL deriva UV da idade total; a integração antiga verificava extensão geométrica, não a região UV. | O novo caminho encaminha `dt` ao grupo de trilhas (`EffectSystem.cpp:1120-1132`); a fixture agora mede a soma U renderizada das quatro regiões. Isso verifica avanço de UV, não paridade visual. |
| Regiões UV do filho | `CMPModelEff::FrameMove` avança o controlador uma vez por atualização (`arquivo/fontes/Engine/src/MPModelEff.cpp:500-540`); para `EFFECT_MODELTEXTURE`, `GetLerpTexture` troca uma região quando `elapsed > frameTime` e zera o residual (`arquivo/fontes/Engine/src/I_Effect.cpp:2539-2568`). | O fallback usa `textureFrameIndex(age, frameTime, count)` (`LegacyEffFormat.h:273-299`), derivado da idade total. | Sob o gate BLAST3, `HistoricalEffTimeline::advanceClock`/`sampleQuad` mantêm índice e resíduo por controlador (`HistoricalEffTimeline.h:26-39,104-121`), atualizados por cada `EffectSystem::step(dt)` e consumidos no renderer (`EffectSystem.cpp:1120-1132,2399-2402`). |

## Fatos, inferências e desconhecidos

**FATOS:** O DX9 reinicia `m_fCurTime` em zero ao avançar o quadro (`arquivo/fontes/Engine/include/MPParticleSys.h:116-129`); move usando o vetor atual antes de acrescentar aceleração (`MPParticleSys.cpp:2655-2658`); o pai BLAST3 usa `FrameMoveBlast3` (`MPParticleSys.cpp:394-399`). A auditoria atual encontrou um sistema BLAST3 com mais de um quadro (`build/sk217ad-fx-coverage/fx_feature_coverage.csv`, linha `blast3_frame_timeline`: 1 instância/1 arquivo/0 skills no grafo rastreável). `top_par_audit` localiza esse sistema em `Client/effect/fushe.par`, `fus2h2e.par`: 8 partículas, 2 quadros, vida 4,5 s e filho `kulou.eff`. `top_eff_inspect Client/effect/kulou.eff` confirma um arquivo v7 válido (493 bytes), um subefeito `EFFECT_MODELTEXTURE`, uma chave de 5 s, quatro regiões, `regionFrameTime=0.1`, um conjunto de UVs e uma textura estática. O renderer escolhe o relógio BLAST3 para as regiões do filho quando os switches estão ligados (`sources/Engine/src/top/EffectSystem.cpp:2202-2216,2399-2402`).

**INFERÊNCIA:** A transformação em Z usa a expansão trigonométrica correspondente à matriz D3DX usada no código. A tradução de coordenadas mantém X e troca o eixo vertical do espaço DX9 para Y no renderer GL; a geometria pode ainda depender de billboard e câmera.

**DESCONHECIDO:** O grafo atual não liga esse `.par` a uma skill; zero ligações não provam que não haja consumidor. A fixture de integração confirma que atualizações discretas do pai alteram as UVs do filho, mas não compara pixels/quadros com o DX9 nem o aspecto no cliente. A inicialização DX9 também sorteia `m_fPartTime` (`MPParticleSys.cpp:2608`), mas o `GetCurFrame` chamado pelo BLAST3 não lê esse campo. Não houve comparação visual DX9/GL.

## Auditoria de recursos

`build/top_fx_coverage_audit --client Client --out build/sk217ad-fx-coverage` terminou com `par=1031`, `eff=1227`, `systems=3476`, `eff_parts=2832`, `gaps=23`, `skills_with_gaps=307`, `drift=0`, `failures=0`. `blast3_frame_timeline` e `blast3_nested_eff_region_clock` registram 1 sistema em 1 arquivo e 0 skills no grafo. `top_par_audit` identifica `fushe.par`/`fus2h2e.par` (8 slots, 2 quadros, vida 4,5 s) e o filho `kulou.eff`; `top_eff_inspect` confirma o filho v7, 493 bytes, completamente decodificado: type 3, uma chave de 5 s, quatro regiões UV por intervalo de 0,1 s e textura estática. Zero ligações no grafo não prova ausência de consumidores.

## Testes executados

- Configuração e compilação Debug completas: `cmake -S sources -B build -DCMAKE_BUILD_TYPE=Debug -DTOP_BUILD_UI_TESTS=ON` e `cmake --build build -j$(nproc)`; `top_gl` compilou.
- `ctest --test-dir build --output-on-failure`: **129/129**.
- `ui05sk217b_extended_particle_integration`: com uma fixture filha `EFFECT_MODELTEXTURE`, quatro regiões UV e intervalo de 0,1 s, confirma que o modo discreto altera as UVs renderizadas pelo filho BLAST3 e mantém o fallback FIRE igual.
- `ui05sk217ad_blast3_update_timeline`: confirma gate desligado por padrão, fórmula da distribuição radial, ordem movimento/aceleração e avanço de no máximo um quadro por atualização.
- O audit de Client foi executado com sucesso; não houve auditoria de screenshots ou comparação visual.

## Ativação e reversão

Ative os dois switches no comando do jogo:

```bash
TOP_HISTORICAL_PARTICLE_EXTENDED=1 \
TOP_HISTORICAL_BLAST3_TIMELINE=1 \
./build/top_gl --profile
```

Para voltar ao renderer anterior, remova `TOP_HISTORICAL_BLAST3_TIMELINE` ou defina-o como `0` e reinicie. Reversão do patch: `git apply -R SK217AD.patch`.

## Limite

O comportamento continua experimental e não está declarado visualmente fiel. A timeline do pai e a troca de regiões UV do filho agora têm cobertura automatizada; comparação visual, curvas/tamanho e interação com o ciclo completo de render ainda precisam de validação contra DX9.
