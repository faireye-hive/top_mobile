# SK2.17-K — corrigir classificação do audit de filtros `.par`

## Achado e comparação

O `fx_gap_ranking.csv` listava `feature:filters` entre os maiores itens e a tabela dizia que o GL apenas lia os filtros, sem aplicá-los por partícula. Essa descrição estava errada: filtro é estado do sistema `.par`, não propriedade individual da partícula. O filtro continua em `fx_feature_coverage.csv` como capacidade inventariada; o ranking de lacunas agora o omite quando o token de runtime está presente.

| Original DX9 (fato) | Porte GL atual (fato) | Mudança desta etapa |
|---|---|---|
| `CMPPartSys` inicia `_eMinFilter` e `_eMagFilter` como `D3DTEXF_POINT` (`arquivo/fontes/Engine/src/MPParticleSys.cpp:829-830`) e aplica ambos como `D3DSAMP_*FILTER` ao desenhar (`MPParticleSys.cpp:1311-1312,1390-1391`). | O porte preserva min/mag no `ParticleProfile`, agrupa batches por esses valores e os torna efetivos apenas com `TOP_HISTORICAL_FILTERS=1` (`sources/Engine/src/top/EffectSystem.cpp:58-65,1733-1745`; `sources/Engine/include/top/HistoricalTextureFilter.h:14-21`). | A linha de cobertura agora consulta `historical_filter::effective`; a observação documenta o mapeamento e deixa claro que a paridade visual continua pendente. O teste sintético verifica que o audit encontra esse uso fora do loader. |
| `D3DTEXF_POINT` seleciona amostragem por ponto; `D3DTEXF_LINEAR` seleciona interpolação linear. | O backend GL usa sampler objects por draw: POINT vira `GL_NEAREST`, LINEAR vira `GL_LINEAR`; não altera o estado de textura compartilhado com HUD/terreno (`sources/Engine/src/gl/GfxDeviceGL.cpp:272-290,484-489`). | Nenhum comportamento de renderização foi alterado. Foi corrigido somente o diagnóstico e incluída uma regressão do audit. |

## Fato, inferência e desconhecido

- **Fato:** existe caminho GL com switch opt-in, batchização por par de filtros e tradução `D3DTEXF_POINT/LINEAR` para `GL_NEAREST/LINEAR`, ancorado nas linhas acima.
- **Inferência:** o item de filtros não deve ser interpretado como mecanismo ausente; o ranking misturava lacunas reais com capacidades já implementadas.
- **Desconhecido:** a aparência final de todos os filtros ainda depende de comparação visual DX9/GL. Valores D3D que não sejam POINT ou LINEAR continuam no fallback linear de compatibilidade (`HistoricalTextureFilter.h:8-12`).

## Auditoria e teste

O audit lê `.par` do Client, consulta o token `historical_filter::effective` no renderer e anota as linhas de referência em `fx_feature_coverage.csv`. O teste usa `.par` sintético com filtros não zero, exige `referenced_outside_loader` e confirma que filtros não aparecem no ranking de lacunas. A contagem do Client é inventário dos recursos; não prova execução das skills.

Comandos usados nesta etapa:

```bash
cmake --build build --target top_fx_coverage_tests top_fx_coverage_audit -j$(nproc)
./build/top_fx_coverage_tests
./build/top_fx_coverage_audit --client Client --out build/sk217j-fx-coverage
```

Resultado executado: build Debug dos alvos de auditoria aprovado; teste fixture `top_fx_coverage_tests` aprovado; CTest completo: 107/107; audit Client concluiu com `par=1031`, `eff=1227`, `systems=3476`, `eff_parts=2832`, `drift=0`, `failures=0`. A contagem de lacunas classificadas caiu de 21 para 20; filtros continuam listados na tabela de cobertura como `referenced_outside_loader` e `not_visually_verified`.

O teste visual anterior continua válido para inspeção A/B:

```bash
TOP_HISTORICAL_FILTERS=1 ./build/top_gl --profile
TOP_HISTORICAL_FILTERS=0 ./build/top_gl --profile
```

Desligue o comportamento com `TOP_HISTORICAL_FILTERS=0`. Para reverter somente esta etapa: `git apply -R SK217K.patch`.
