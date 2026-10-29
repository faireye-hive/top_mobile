# SK2.17-X — emissão, vida por slot e reciclagem do RIPPLE

## Resumo

O audit do Client encontrou 23 sistemas `type=4` em 12 `.par`; 21 têm `step>0` e mais de um quadro, distribuídos em 11 arquivos e alcançando 21 skills. Esses 21 dados medidos acionam `feature:ripple_emission_recycle`. O modo novo é opt-in por `TOP_HISTORICAL_RIPPLE_POOL=1`.

## Evidência: DX9 original x porte GL x mudança

| Aspecto | DX9 original | Porte OpenGL antes/depois | Mudança |
|---|---|---|---|
| Começo do emissor | `CMPPartSys::Play` configura `_fCurTime = _fStep` (`arquivo/fontes/Engine/src/MPParticleSys.cpp:885-894`). | O porte não tinha estado de emissor dedicado ao RIPPLE (`sources/Engine/src/top/EffectSystem.cpp:1868-1891`). | Timeline opt-in inicializa o mesmo timer antecipado (`sources/Engine/include/top/HistoricalRipple.h:15-26`). |
| Ordem de emissão e reciclagem | `_CreateRipple` acumula `dt` no timer compartilhado, zera-o ao atingir `step`, preenche o slot morto e sorteia vida entre `_fLife/_iRoadom` e `_fLife` (`arquivo/fontes/Engine/src/MPParticleSys.cpp:3967-3997`). `_FrameMoveRipple` visita slots em ordem; slot que morreu só pode renascer em chamada posterior (`:4013-4033,4079-4084`). | O porte emitia a contagem de quads com idades baseadas em `particleAge + particle*step`; RIPPLE não usava `SystemTimeline` (`sources/Engine/src/top/EffectSystem.cpp:1947-1987`). | O novo estado visita os slots em ordem, compartilha o timer, aplica o divisor decodificado à vida e adia reciclagem para a próxima atualização (`HistoricalRipple.h:29-58`). O `rand()` global do DX9 é substituído por valores determinísticos por slot/generation. |
| Curvas e cadência | RIPPLE chama `GetCurFrame` para cada slot e atualiza tamanho/cor; o cursor DX9 avança uma vez e descarta residual (`arquivo/fontes/Engine/src/MPParticleSys.cpp:4013-4045`; `arquivo/fontes/Engine/include/MPParticleSys.h:116-129`). | GL usava amostra analítica pela idade comum ao sistema (`EffectSystem.cpp:1847-1859`). | Cada slot agora expõe fase calculada de `frame` + fração residual; a curva de tamanho/cor é amostrada com essa fase (`HistoricalRipple.h:60-75`; `EffectSystem.cpp:1983-1991`). |
| Coordenadas | `Randf(rangeX/Y/Z)` adiciona offsets não negativos ao ponto do emissor (`arquivo/fontes/Engine/src/MPParticleSys.cpp:3974-3982`). | O GL é Y-up; a posição RIPPLE é desenhada no plano do chão (`sources/Engine/src/top/EffectSystem.cpp:2375-2378`). | No modo histórico, DX9 `(x,y,z-up)` é traduzido para GL `(x,z,y-up)` na posição de range (`sources/Engine/src/top/EffectSystem.cpp:2062-2074`). O caminho antigo permanece quando a opção está desligada. |
| Filho `.eff` | `_FrameMoveRipple` chama `CMPModelEff::FrameMove` para o filho uma vez por atualização (`arquivo/fontes/Engine/src/MPParticleSys.cpp:4009-4012`). | O render suporta os subefeitos, mas os quadros ainda vêm de `legacy_eff::framePhase(part, modelAge)` (`sources/Engine/src/top/EffectSystem.cpp:2113-2152`). | Sem mudança nesta etapa. O audit separa os sete sistemas com filhos `.eff` para não esconder essa lacuna restante. |

## Escopo medido e limites dos dados

`top_par_audit` leu 1.031 `.par`: 3.476 sistemas, zero falhas. Para type 4, as 23 linhas decodificadas têm zero `model_range` e zero `has_path`; sete referenciam modelo `.eff`. Isso limita o conjunto observado, não prova que o mecanismo DX9 nunca receba caminho em runtime. O audit não registra `IsBone`, portanto não há conclusão sobre esse estado. O divisor de vida é obtido do formato `.par`; os valores aleatórios e a ordem global de `rand()` não são reproduzidos.

`top_fx_coverage_audit` Client: 1.031 `.par`, 1.227 `.eff`, 3.476 sistemas, 2.832 subefeitos, 26 lacunas, 307 skills com lacunas, `drift=0`, `failures=0`. `ripple_emission_recycle`: 21 sistemas, 11 arquivos, 21 skills. `ripple_nested_eff_frame_step`: 7 sistemas, 7 arquivos, 6 skills; seu relógio filho continua analítico no GL. `fx_decode_failures.csv` tem somente cabeçalho; nenhum `.eff` falhou nesta execução e não há item a descartar como sem uso.

## Alterações

- Novo `historical_ripple::SystemTimeline` guarda timer compartilhado, estado/idade/vida/quadro por slot e geração (`sources/Engine/include/top/HistoricalRipple.h`).
- `EffectSystem::step` avança RIPPLE apenas com a opção histórica; o render suprime slots ainda não nascidos e usa idade/curva por slot (`sources/Engine/src/top/EffectSystem.cpp`).
- `FxCoverageAudit` deriva a feature de `type==4 && step>0`, e o fixture do auditor verifica a presença do novo recurso no ranking.
- A fixture de integração verifica que o caminho legado continua desenhando os dois slots e que o modo histórico inicia com apenas o slot autorizado pelo timer DX9.

## Compilação e testes executados

```bash
cmake -S sources -B build -DCMAKE_BUILD_TYPE=Debug -DTOP_BUILD_UI_TESTS=ON
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

Resultado observado: configuração e build Debug, incluindo `top_gl`, concluídos com exit code 0; CTest **126/126**. Testes unitário, integração do gate e regressão do FX audit passaram. O teste unitário passou com ASan/UBSan (`ASAN_OPTIONS=detect_leaks=0`).

Audit reproduzível:

```bash
./build/top_fx_coverage_audit --client Client --out build/sk217x-final-fx-audit \
  --deps sources/auditoria-historica/dependencies.csv \
  --table sources/auditoria-historica/fx_port_coverage.csv \
  --source sources/Engine/src/top/EffectSystem.cpp
```

## Validação visual

Não houve comparação de imagem com DX9. O teste integrado confirma roteamento do switch e quantidade inicial de slots no fixture, não fidelidade visual completa. Escala, geometria do RIPPLE, `IsBone`, eventos de parada em runtime e stream global de RNG seguem sem prova visual.

Para desligar: `unset TOP_HISTORICAL_RIPPLE_POOL`. Para reverter esta etapa: `git apply -R SK217X.patch`.

## Checklist visual

Compare um dos `.par` RIPPLE auditados no DX9 e GL: emissão inicial, intervalos `step`, diferença de vida por slot, reciclagem, último quadro e parada do emissor. No GL, use `TOP_HISTORICAL_RIPPLE_POOL=1` e repita sem a variável; mantenha iguais as opções históricas restantes do ensaio.
