# SK2.17-AQ — keyframes dos canais Item/ItemExtra

Data: 2026-10-25. Mudança aditiva, experimental e desligada por padrão.

## Evidência: DX9 × OpenGL

| Original DX9 | Porte atual antes desta etapa | Mudança SK2.17-AQ |
|---|---|---|
| `arquivo/fontes/Client/src/STAttack.cpp:217-229` define os callbacks de início `-1`, fim `-2` e normaliza callback positivo para `0`. Em `:262-263`, Item1/Item2 são disparados somente quando o keyframe próprio da coluna coincide com o callback. | `sources/Engine/src/top/Play.cpp:1577-1585` já lia ID/keyframe das colunas Item/ItemExtra; `sources/Engine/src/top/EffectSystem.cpp:784-808` enviava todos os canais de cast juntos. `sources/Engine/src/top/play/PlayCombat.cpp:197-212,840-855,1098-1113` agendava um único evento de cast pela ação. | `TOP_HISTORICAL_ITEM_EFFECT_KEYFRAME=1` agenda callbacks `-1`, `0`, `-2` e filtra os canais pela chave gravada. O caminho padrão mantém o evento de cast combinado legado. |
| `arquivo/fontes/Libraries/common/src/SkillRecord.cpp:391-407` lê as listas Item1/Item2 (ID e keyframe) separadamente. | A tabela já estava carregada, mas a temporização Item/ItemExtra não era consumida pelo escalonador. | `sources/Engine/include/top/Play.h:453` retém os keyframes; `sources/Engine/src/top/Play.cpp:1578-1585` os lê; `PlayCombat.cpp:22-40,199-212,843-855,1100-1113` agenda por callback; `EffectSystem.cpp:787-824` seleciona os canais correspondentes. |

Tradução de convenção: não há estado D3D ou chamada de renderização nesta mudança. Os inteiros de callback DX9 são preservados como semântica de evento. O porte mapeia `-1` para início, `0` para o quadro de ação já estimado e `-2` para o fim da duração da ação no relógio OpenGL. Essa conversão evita copiar API D3D; a duração do callback `0` ainda depende do relógio/impacto aproximado existente no porte.

## Medição no Client

Auditoria executada em `Client/` com o executável `build/top_fx_coverage_audit`, saída `build/sk217aq-fx-audit/`. O `fx_skill_item_channels.csv` contém 50 canais de item não nulos em 46 skills. Quatro canais Item1 têm chave `-1`, distintos do callback de ação `0`: skill 0094 Cripple, 0095 Enfeeble, 0096 Headshot e 0327 Headshot Illusion. Os quatro referenciam sceneffect ID 256, `01000016.par` (tipos candidatos `12;6`). Cripple também tem ItemExtra ID 142 em `0`. `runtime_use_proven=no`: tabela e referências provam autoria/dados, não uma execução observada no cliente.

Auditoria de recursos da mesma execução: 1.031 PAR, 1.227 EFF, 3.476 sistemas PAR, 2.832 partes EFF, 26 lacunas reportadas, 307 skills com lacunas, 0 falhas gerais de decodificação e 31 falhas do decodificador estrito EFF. O código DX9 `CMPResManger::LoadTotalEffect` percorre `*.eff`, chama `LoadEffectFromFile`, indexa pelo nome e avança o catálogo (`arquivo/fontes/Engine/src/MPResManger.cpp:1351-1405`). Logo, os 31 arquivos não são dispensáveis para reproduzir a fase histórica de carregamento, mesmo quando o audit não encontra uma ligação PAR direta. Trinta também têm referências diretas por PAR/sceneffectinfo. O 31º, `fboomerff.eff`, é associado ao `fboomerff.par` de mesmo nome; sceneffect ID 919 aponta para esse PAR, e a skill 0308 Bomb Throwing usa o ID 919 (`Client/scripts/table/sceneffectinfo.txt:551`, `skillinfo.txt:253`). Isso comprova relevância do conjunto de dados e carregamento do `.eff` pelo catálogo DX9, mas não comprova que seus modelos sejam exibidos nessa skill ou em uma sessão. Não removemos nem ignoramos nenhum dos 31; o modo diagnóstico atual estrutura todos. A rotina de carregamento dos parâmetros de grupo em `CEffectScript::LoadParamFromName` é um sistema relacionado, mas separado (`arquivo/fontes/Engine/src/MPEffectCtrl.cpp:86-110`, chamada em `arquivo/fontes/Client/src/EffectObj.cpp:922-927`). CSVs detalhados: `build/sk217aq-fx-audit/fx_eff_usefulness.csv` e `fx_decode_failures.csv`.

## Comportamento implementado e limites

- Chave do canal item é separada da chave de ação e canais duplicados na mesma chave são agrupados num callback.
- A seleção de canal é aplicada aos casts do jogador, aos casts próprios e aos casts de inimigos que passam pelo escalonador existente.
- IDs de keyframe fora do domínio `[-2,0]` não são agendados pela nova rota. Nenhum dado do Client desta medição exige domínio adicional.
- A execução por inimigos usa o mesmo evento de cast histórico; `sourceIsPlayer` continua diferenciando ownership dos canais presos ao personagem/item.
- Nenhum recurso é criado como substituto. A correção de precedência `.eff` sobre `.lgo` do Frozen Arrow não é tocada.

## Testes e comandos

Build Debug executado com sucesso; CTest completo: 136/136 aprovados. O teste `ui05v_play_state_regression` verifica dados reais de Cripple, agendamento de `-1` antes de `0`, filtro por canal e modo padrão desligado. Há também o teste de composição dos canais da skill.

```bash
cmake -S sources -B build -DCMAKE_BUILD_TYPE=Debug -DTOP_BUILD_UI_TESTS=ON
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure

TOP_HISTORICAL_ITEM_EFFECT_KEYFRAME=1 \
TOP_HISTORICAL_BLEND=1 TOP_HISTORICAL_MODEL_TEXTURE=1 \
TOP_FROZEN_RESULT_GROUND=1 ./build/top_gl --profile
```

Desligar: reinicie sem `TOP_HISTORICAL_ITEM_EFFECT_KEYFRAME` ou com valor `0`. Reverter este patch incremental: `git apply -R SK217AQ.patch`.

## Fatos, inferências e desconhecidos

- **FATO:** código original e dados suportam callbacks Item separados; quatro skills medidas usam Item1 em `-1` enquanto a ação está em `0`.
- **FATO:** o porte OpenGL anterior despachava Item/ItemExtra em um único evento de cast; agora existe seleção por callback quando o switch está ligado.
- **INFERÊNCIA:** o atraso do callback `0` corresponde visualmente ao mesmo ponto de ação se a pose/impacto medido no porte acompanha o callback de animação DX9.
- **DESCONHECIDO:** igualdade de tempo/pose em runtime, resultado visual no GL contra DX9, e se `fboomerff.eff` é realmente executado. Não houve comparação visual do usuário nem execução observada desses quatro efeitos.
