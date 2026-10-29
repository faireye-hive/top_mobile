# SK2.17-BN — utilidade individual dos `.eff` recusados pelo decoder estrito

## Resultado

Reexecutei o `top_fx_coverage_audit` sobre `Client/` e sobre a fonte DX9. A saída está em `build/sk217cn-fx-audit/`. O Client contém 1.031 PARs, 1.227 EFFs, 3.476 sistemas PAR e 2.832 partes EFF. O FX audit registra 31 recusas strict-only; `fx_decode_failures.csv` não tem falhas estruturais e as 31 entradas de `fx_eff_usefulness.csv` foram recuperadas pelo parser diagnóstico com parâmetros de modelo crus.

**Resposta prática:** não encontrei um `.eff` que possa ser classificado com evidência como dispensável. Trinta arquivos são referenciados diretamente por um PAR; o outro, `fboomerff.eff`, não tem aresta direta, mas tem `fboomerff.par` de mesmo nome-base no catálogo (sceneffectinfo ID 919 e skill estática 308). A última associação não prova uso runtime. Preservei os 31; não removi recursos nem alterei o decoder.

O DX9 percorre `*.eff` e tenta `LoadEffectFromFile` para cada arquivo (`arquivo/fontes/Engine/src/MPResManger.cpp:1351-1405`). Mesmo se essa chamada falha, o trecho continua e registra o nome/índice; isso comprova tentativa de carga/indexação, não carga bem-sucedida. O original grava e lê `segments`, `height`, `radius` e `botRadius` como floats sem teste de finitude (`arquivo/fontes/Engine/src/I_Effect.cpp:517-520,662-669`). Isso explica por que o parser diagnóstico precisa preservar os valores crus; não prova que NaN/Inf sejam seguros para geometria ou que fossem intencionais.

## Inventário por arquivo

Todos os 31 registros têm `strict_decode=failed`, `diagnostic_decode=decoded_with_raw_model_params`, `dx9_catalog_load_evidence=enumerated_loaded_indexed` e `runtime_use_proven=no`. `nonfinite_animated_model_params=0` em todos; o número da coluna “parâmetros crus” é a contagem dos parâmetros base não finitos. A contagem de partes/modelos abaixo vem do decode diagnóstico.

| `.eff` | Vínculo estático | Partes (modelos) | Parâmetros crus | Conclusão de uso |
|---|---|---|---:|---|
| `effect/fboomerff.eff` | Sem aresta direta; PAR homônimo effect/fboomerff.par; catálogo ID 919; skill estática 308 | 3 (RectPlane;boom.lgo) | 3 | Associação sugestiva; uso runtime não provado. |
| `effect/fjl803.eff` | PAR: effect/fjl803.par; catálogo ID 1005 | 1 (8yuejl01.lgo) | 3 | Recurso referenciado nos dados; runtime não provado. |
| `effect/jbbg01.eff` | PAR: effect/jbbg01.par; catálogo ID 3610 | 2 (RectPlane) | 2 | Recurso referenciado nos dados; runtime não provado. |
| `effect/jbbg02.eff` | PAR: effect/jbbg02.par; catálogo ID 3611 | 2 (RectPlane) | 2 | Recurso referenciado nos dados; runtime não provado. |
| `effect/jbbg03.eff` | PAR: effect/jbbg03.par; catálogo ID 3612 | 2 (RectPlane) | 2 | Recurso referenciado nos dados; runtime não provado. |
| `effect/jbbg04.eff` | PAR: effect/jbbg04.par; catálogo ID 3613 | 5 (Cylinder;RectPlane) | 2 | Recurso referenciado nos dados; runtime não provado. |
| `effect/jbg01.eff` | PAR: effect/jbg01.par; catálogo ID 3640 | 2 (RectPlane) | 2 | Recurso referenciado nos dados; runtime não provado. |
| `effect/jbg02.eff` | PAR: effect/jbg02.par; catálogo ID 3641 | 2 (RectPlane) | 2 | Recurso referenciado nos dados; runtime não provado. |
| `effect/jbg03.eff` | PAR: effect/jbg03.par; catálogo ID 3642 | 2 (RectPlane) | 2 | Recurso referenciado nos dados; runtime não provado. |
| `effect/jbg04.eff` | PAR: effect/jbg04.par; catálogo ID 3643 | 5 (Cylinder;RectPlane) | 2 | Recurso referenciado nos dados; runtime não provado. |
| `effect/jbg05.eff` | PAR: effect/jbg02.par;effect/jbg03.par;effect/jbg04.par; catálogo ID 3641;3642;3643 | 1 (RectPlane) | 1 | Recurso referenciado nos dados; runtime não provado. |
| `effect/jbrg01.eff` | PAR: effect/jbrg01.par; catálogo ID 3550 | 2 (RectPlane) | 2 | Recurso referenciado nos dados; runtime não provado. |
| `effect/jbrg02.eff` | PAR: effect/jbrg02.par; catálogo ID 3551 | 2 (RectPlane) | 2 | Recurso referenciado nos dados; runtime não provado. |
| `effect/jbrg03.eff` | PAR: effect/jbrg03.par; catálogo ID 3552 | 2 (RectPlane) | 2 | Recurso referenciado nos dados; runtime não provado. |
| `effect/jbrg04.eff` | PAR: effect/jbrg04.par; catálogo ID 3553 | 5 (Cylinder;RectPlane) | 2 | Recurso referenciado nos dados; runtime não provado. |
| `effect/jbyg01.eff` | PAR: effect/jbyg01.par; catálogo ID 3580 | 2 (RectPlane) | 2 | Recurso referenciado nos dados; runtime não provado. |
| `effect/jbyg02.eff` | PAR: effect/jbyg02.par; catálogo ID 3581 | 2 (RectPlane) | 2 | Recurso referenciado nos dados; runtime não provado. |
| `effect/jbyg03.eff` | PAR: effect/jbyg03.par; catálogo ID 3582 | 2 (RectPlane) | 2 | Recurso referenciado nos dados; runtime não provado. |
| `effect/jjbg02.eff` | PAR: effect/jjbg02.par; catálogo ID 3621 | 9 (Rect;RectPlane) | 5 | Recurso referenciado nos dados; runtime não provado. |
| `effect/jjbg03.eff` | PAR: effect/jjbg03.par; catálogo ID 3622 | 13 (Cylinder;Rect;RectPlane) | 5 | Recurso referenciado nos dados; runtime não provado. |
| `effect/jjbg04.eff` | PAR: effect/jjbg04.par; catálogo ID 3623 | 15 (Cylinder;Rect;RectPlane) | 5 | Recurso referenciado nos dados; runtime não provado. |
| `effect/jjg02.eff` | PAR: effect/jjg02.par; catálogo ID 3651 | 9 (Rect;RectPlane) | 5 | Recurso referenciado nos dados; runtime não provado. |
| `effect/jjg03.eff` | PAR: effect/jjg03.par; catálogo ID 3652 | 13 (Cylinder;Rect;RectPlane) | 5 | Recurso referenciado nos dados; runtime não provado. |
| `effect/jjg04.eff` | PAR: effect/jjg04.par; catálogo ID 3653 | 15 (Cylinder;Rect;RectPlane) | 5 | Recurso referenciado nos dados; runtime não provado. |
| `effect/jjrg02.eff` | PAR: effect/jjrg02.par; catálogo ID 3561 | 9 (Rect;RectPlane) | 5 | Recurso referenciado nos dados; runtime não provado. |
| `effect/jjrg03.eff` | PAR: effect/jjrg03.par; catálogo ID 3562 | 13 (Cylinder;Rect;RectPlane) | 5 | Recurso referenciado nos dados; runtime não provado. |
| `effect/jjrg04.eff` | PAR: effect/jjrg04.par; catálogo ID 3563 | 15 (Cylinder;Rect;RectPlane) | 5 | Recurso referenciado nos dados; runtime não provado. |
| `effect/jjyg02.eff` | PAR: effect/jjyg02.par; catálogo ID 3591 | 9 (Rect;RectPlane) | 5 | Recurso referenciado nos dados; runtime não provado. |
| `effect/jjyg03.eff` | PAR: effect/jjyg03.par; catálogo ID 3592 | 13 (Cylinder;Rect;RectPlane) | 5 | Recurso referenciado nos dados; runtime não provado. |
| `effect/jjyg04.eff` | PAR: effect/jjyg04.par; catálogo ID 3593 | 15 (Cylinder;Rect;RectPlane) | 5 | Recurso referenciado nos dados; runtime não provado. |
| `effect/ttd.eff` | PAR: effect/ttdan.par; catálogo ID 591 | 1 (ttd.lgo) | 1 | Recurso referenciado nos dados; runtime não provado. |

## Fatos, inferências e desconhecidos

- **FATO:** 30/31 arquivos têm aresta direta PAR; são 32 referências PAR somadas. Nenhum registro comprova execução de runtime (`fx_eff_usefulness.csv`).
- **FATO:** `fboomerff.eff` não tem aresta direta; o PAR homônimo aparece no catálogo com ID 919/skill estática 308. Essa associação é catálogo/dado, não prova de que esse `.eff` específico seja solicitado.
- **FATO:** o DX9 enumera `*.eff`, tenta abrir cada um e adiciona nome/índice mesmo após falha de `LoadEffectFromFile` (`MPResManger.cpp:1359-1405`).
- **FATO:** o formato DX9 lê/escreve os três floats do modelo sem validar NaN/Inf (`I_Effect.cpp:517-520,662-669`).
- **INFERÊNCIA:** os 30 arquivos referenciados são dependências úteis do grafo de recursos e devem continuar decodificáveis para preservar esses PARs; isso não afirma que todas as referências sejam ativadas no gameplay.
- **DESCONHECIDO:** se `fboomerff.eff` é chamado em alguma sessão; se parâmetros não finitos são intencionais ou artefatos; o resultado visual DX9 e GL para cada arquivo.

## Testes, execução e próximos passos

Esta é uma etapa somente de auditoria; não alterou C++ nem arquivos do Client. A execução do FX audit retornou exit code 0. O audit completo reportou 25 lacunas/307 IDs de skill estáticos, drift 0 e zero falhas estruturais; visual parity permanece `false`. Não executei o jogo nem comparação visual.

O switch preexistente `TOP_HISTORICAL_EFF_RAW_MODEL_PARAMS=1` continua sendo opt-in para aceitar somente esses parâmetros crus; o default e as demais validações permanecem inalterados. Próxima investigação útil: descobrir no código de carregamento/uso se há chamada de runtime para `fboomerff.eff`, sem inferir isso apenas do PAR homônimo. Para os 30 referenciados, eventual correção visual requer comparação com os recursos no cliente.
