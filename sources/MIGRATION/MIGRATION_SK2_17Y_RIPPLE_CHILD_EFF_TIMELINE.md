# SK2.17-Y — relógio histórico dos `.eff` filhos do RIPPLE

## Resumo

O audit Client anterior encontrou sete sistemas RIPPLE com `.eff` filho em sete `.par`, alcançando seis skills; os quatro arquivos filhos distintos inspecionados decodificaram. No DX9, `_FrameMoveRipple` avança o `CMPModelEff` uma vez por atualização, independentemente da quantidade de slots vivos. O GL agora pode reproduzir o avanço discreto e os relógios internos do filho ao ativar `TOP_HISTORICAL_RIPPLE_POOL=1`.

## Evidência: DX9 x GL x mudança

| Comportamento | Original DX9 | Porte GL antes/depois | Mudança |
|---|---|---|---|
| Início e ciclo | `CMPPartSys::Play(RIPPLE)` prepara o emissor e chama `_CPPart->Play(0)`, onde `0` significa loop (`arquivo/fontes/Engine/src/MPParticleSys.cpp:885-905`; `arquivo/fontes/Engine/include/MPModelEff.h:377-404`). | A vida das partículas já tinha estado RIPPLE, mas os quadros do EFF filho continuavam usando `legacy_eff::framePhase(part, modelAge)` (`sources/Engine/src/top/EffectSystem.cpp:1097-1111,2162-2169`). | Uma `historical_eff_timeline::Group` por perfil atualiza em conjunto e reinicia em loop quando todos os subefeitos param (`sources/Engine/src/top/EffectSystem.cpp:1107-1109`; regra de grupo em `sources/Engine/include/top/HistoricalEffTimeline.h:54-95`). |
| Cadência de quadros | O pai chama `CMPModelEff::FrameMove(dt)` uma vez antes de percorrer os slots (`arquivo/fontes/Engine/src/MPParticleSys.cpp:4007-4018`). Cada controlador acrescenta `dt`, avança no máximo um quadro e zera o residual (`arquivo/fontes/Engine/src/MPModelEff.cpp:514-540`). | Idade analítica por partícula podia avançar vários quadros de uma vez e concluir o filho sem preservar a cadência de chamadas (`sources/Engine/src/top/EffectSystem.cpp:2162-2169`). | O mesmo `activeDelta` da atualização do perfil alimenta um grupo, não cada slot; o estado por filho avança no máximo um quadro e descarta o residual (`sources/Engine/include/top/HistoricalEffTimeline.h:54-83`). |
| Pose e dados animados | DX9 interpola tamanho, posição, cor e, conforme o tipo, coordenadas ou textura em `FrameMove` (`arquivo/fontes/Engine/src/MPModelEff.cpp:548-589`). | O GL já renderizava geometria/textura do `.eff`, mas derivava a fase, textura e UV do tempo analítico (`sources/Engine/src/top/EffectSystem.cpp:2166-2178,2255-2258,2321-2342`). | RIPPLE usa fase, textura e amostras UV do grupo; STRIP mantém os gates existentes (`sources/Engine/src/top/EffectSystem.cpp:2130-2141,2166-2169,2192-2198,2255-2258,2321-2342`). |

O relógio é um cursor lógico do efeito, não uma cópia de Direct3D. A geometria e os dados continuam a ser renderizados pelo caminho OpenGL existente. O `Play(0)` do DX9 é traduzido por `loop=true`; a seleção por chamada e o descarte do residual preservam as regras do controlador, enquanto as amostras de pose alimentam o renderer GL.

## `.eff` que falharam e uso dos arquivos

**FATO:** a auditoria Client refeita nesta etapa encontrou 1.031 `.par`, 1.227 `.eff`, 3.476 sistemas, 2.832 subefeitos, zero falhas de decodificação e zero deriva de tabelas. `fx_decode_failures.csv` contém apenas o cabeçalho. Os sete vínculos RIPPLE são referências em sete `.par`; os quatro nomes distintos inspecionados foram `01000028(a).eff`, `01000029(b).eff`, `010300111.eff` e `whurt(a).eff`, e todos decodificaram.

**INFERÊNCIA:** como essas referências são carregadas pelo sistema de partículas e consumidas por `FrameMove`/render, são recursos usados e não devem ser descartados como arquivos sem uso.

**DESCONHECIDO:** uma contagem de referências não prova que cada habilidade foi executada em runtime no cliente. O ambiente atual não foi comparado visualmente quadro a quadro com DX9.

## Alterações

- Adicionado estado `rippleEffTimelines` separado de `stripEffTimelines` por instância e perfil (`sources/Engine/include/top/EffectSystem.h:201-205`).
- A etapa de simulação atualiza o grupo filho uma vez por perfil RIPPLE e só com `TOP_HISTORICAL_RIPPLE_POOL=1` (`sources/Engine/src/top/EffectSystem.cpp:1066-1071,1097-1111`).
- O renderer escolhe a timeline RIPPLE para quadros, textura e controladores UV; o caminho anterior continua ativo quando a opção está desligada (`sources/Engine/src/top/EffectSystem.cpp:2121-2141,2166-2178,2192-2198,2255-2258,2321-2342`).
- O FX audit não lista mais `ripple_nested_eff_frame_step` como lacuna de código, e o teste de auditoria confirma essa classificação (`sources/Engine/include/top/FxCoverageAudit.h`; `sources/auditoria-historica/fx_port_coverage.csv`; `sources/tests/FxCoverageAuditTests.cpp`).
- Fixture de integração com `.par`/`.eff` sintéticos diferencia o quadro analítico do avanço discreto limitado a uma etapa por update (`sources/tests/SK217YRippleEffIntegrationTests.cpp`).

## Compilação e testes executados

```bash
cmake -S sources -B build -DCMAKE_BUILD_TYPE=Debug -DTOP_BUILD_UI_TESTS=ON
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

Resultado observado: configuração Debug e build completo, incluindo `top_gl`, terminaram com exit code 0. CTest terminou com **127/127** testes aprovados. O teste novo é `ui05sk217y_ripple_eff_timeline`; `ui05sk214_fx_coverage_audit` também passou após atualizar a classificação da lacuna. A fixture integrada foi compilada e executada também com ASan/UBSan (`ASAN_OPTIONS=detect_leaks=0`), exit code 0. A auditoria Client terminou com 25 lacunas e 307 skills com lacunas; `ripple_nested_eff_frame_step` deixou o ranking porque a timeline agora está implementada opt-in.

## Ativação, reversão e limites

Ative com `TOP_HISTORICAL_RIPPLE_POOL=1`; desligue com `unset TOP_HISTORICAL_RIPPLE_POOL`. O padrão permanece desligado. Para reverter somente esta etapa, aplique `git apply -R SK217Y.patch` na raiz.

O fixture testa o relógio, não prova fidelidade visual. A posição/rotação em convenções DX9 versus GL, RNG global, casos `IsBone`, regras exatas de parada em runtime e comparação de imagens com o DX9 seguem pendentes. Nenhuma escala foi aumentada e nenhum efeito substituto foi criado.

## Checklist visual

Compare ao menos um dos sete RIPPLE com filho `.eff` no DX9 e no GL, usando a mesma habilidade, câmera e sequência de `dt`. No GL, repita com `TOP_HISTORICAL_RIPPLE_POOL=1` e desligado. Confira avanço de quadro em updates longos, loop do grupo, quadros dos subefeitos curtos, pose/cor, textura/UV e convivência com emissão/reciclagem RIPPLE.
