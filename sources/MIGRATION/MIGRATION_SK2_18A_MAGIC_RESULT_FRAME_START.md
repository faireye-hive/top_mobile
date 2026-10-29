# SK2.18-A — iniciar o resultado mágico no quadro de chegada

## Resumo

O resultado de Frozen Arrow (skill 93, MagicSingle ID 20, `icearrowh.par`) começava no GL com a fração de `dt` que excedia o fim do voo. O DX9 inicia o controlador de resultado em `Stop()` e chama `FrameMove(0)` ao renderizá-lo. `TOP_HISTORICAL_MAGIC_RESULT_FRAME_START=1` agora inicia o resultado em idade zero, desligado por padrão.

## Evidência: original DX9 × porte GL × mudança

| Original DX9 | Porte GL antes | Mudança |
|---|---|---|
| `CMagicCtrl::Stop()` posiciona o `.par` de resultado em `_vPos`, passa `_vDir` e chama `Play(1)` (`arquivo/fontes/Engine/include/MPEffectCtrl.h:418-425`). Quando parado, `CMagicCtrl::Render()` avança o resultado com `FrameMove(0)` e então o desenha (`arquivo/fontes/Engine/src/MPEffectCtrl.cpp:608-617`). | Ao cruzar o fim do voo, `EffectSystem::step(dt)` cria a instância no mesmo update e atribuía `max(0, flight.age-flight.life)` à idade (`sources/Engine/src/top/EffectSystem.cpp:1412-1416,1440-1442`). Assim, por exemplo, um passo que cruza o voo em 0,26 s já consumia 0,26 s do atraso e animação do resultado. | `TOP_HISTORICAL_MAGIC_RESULT_FRAME_START=1` escolhe idade zero ao criar o resultado; ausente/0 conserva a idade anterior (`EffectSystem.cpp:93-96,1441-1442`). O voo ainda termina na trajetória DX9 já calculada e o resultado continua posicionado na chegada. |

## Dados do Client e auditorias

- `dependencies.csv` liga a skill 93/Frozen Arrow ao voo Magic 20 e ao `Result` `icearrowh.par`; a variante `result_model` aponta para `icearrowh.eff` (`sources/auditoria-historica/dependencies.csv:151-154,1139-1140`). São vínculos estáticos, não prova de execução.
- Reexecutei `top_par_audit` no `Client/`: **1.031/1.031 PAR decodificados**, 3.476 sistemas, zero falhas PAR. `icearrowh.par` é versão 14 e tem três sistemas; o primeiro é type 6 com `.eff` filho, atraso 0,1 s e play time 2,8 s (`build/sk218a-par-audit/par_files.csv`, `par_systems.csv`, `par_dependencies.csv`).
- Reexecutei `top_fx_coverage_audit` depois da mudança: 1.031 PAR, 1.227 EFF, 3.476 sistemas, 2.832 partes, 27 gaps/307 IDs estáticos, `drift=0`, `failures=0`. `fx_decode_failures.csv` contém somente cabeçalho. A lista separada `fx_eff_usefulness.csv` conserva 31 recusas do decoder estrito, recuperadas no parser diagnóstico; 30 têm referência direta PAR e `fboomerff.eff` segue sem consumidor estático conhecido, conforme SK2.17-BN/BO. Nenhum recurso foi removido nem mudou de decoder.

## Fatos, inferência e desconhecido

- **FATO:** o código DX9 inicia `_pPartResult` em `Stop()` e chama `FrameMove(0)` antes do render do resultado. O Client associa o resultado Frozen Arrow à `icearrowh.par` com atraso de 0,1 s no controlador type 6.
- **FATO:** o GL anterior carregava o excesso temporal do passo que cruza o fim do voo para a idade inicial do resultado.
- **INFERÊNCIA:** zerar essa idade ao iniciar aproxima melhor a fronteira de atualização DX9 e evita saltar um atraso/animação em um quadro longo. O teste sintético força um atraso de 0,25 s e um overshoot de 0,26 s para tornar a diferença observável.
- **DESCONHECIDO:** o `dt` efetivo do DX9 no quadro de chegada e a correspondência visual do resultado em execução real. A mudança não certifica a aparência de Frozen Arrow nem de outras skills.

## Testes executados

- Configuração e build Debug completos, incluindo `top_gl`: exit code 0.
- CTest completo: **141/141**.
- Testes focais `ui05g_skill_channel_composition` e `ui05sk24_magic_result_impact`: **2/2**.
- Fixture `top_ui05g_tests --fixture-only` em ASan/UBSan: exit code 0 (`ASAN_OPTIONS=detect_leaks=0`, `UBSAN_OPTIONS=halt_on_error=1`; LSan não executado).
- `top_par_audit` e `top_fx_coverage_audit` contra o Client atual: exit code 0.
- Não executei o jogo nem comparei quadros DX9/OpenGL. Não declarar fidelidade visual.

## Ativação, desativação e reversão

```bash
TOP_HISTORICAL_MAGIC_RESULT_FRAME_START=1 ./build/top_gl --profile
```

Para voltar à idade anterior, remova a variável ou defina `TOP_HISTORICAL_MAGIC_RESULT_FRAME_START=0` e reinicie o jogo. Em uma cópia onde o patch foi aplicado, reverta com `git apply -R SK218A.patch`.

## Próxima lacuna medida

O ranking atual mantém type 6 no topo: 1.041 sistemas, 504 PARs e 242 IDs estáticos; sua classe continua `partial`. O próximo passo deve escolher um submecanismo type 6 verificável no DX9 e nos arquivos PAR/EFF antes de mudar a geometria ou sincronização. Contagens estáticas não provam uso em runtime.
