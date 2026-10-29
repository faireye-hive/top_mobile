# Tales of Pirates offline — Migração 05-AH: modelos REALMENTE utilizados nos mapas

Base: **05-AG**, que já possui a correção dos nomes históricos no Linux. A correção anterior de armas/equipamentos nas mãos **já estava pronta** e não foi modificada.

## Motivação comprovada

O inventário `05AF_assets(1).json` enviado pelo usuário indica 53 nomes de mapas, 52 mapas válidos, 47 `.obj` presentes (43 com grafia exata e quatro com capitalização diferente), seis `.obj` opcionais ausentes, 1182 referências de `.lmo` em `sceneobjinfo.txt`, das quais 606 não estão fisicamente em `model/scene/`. Esses 606 não podem ser classificados como defeitos de mapas sem antes ler as referências reais por setor nos `.obj`.

Os logs da 05-AG enviados pelo usuário confirmam carregamento de `dreamisland`, `pkmap` e `sdboss`, todos com **modelos falhos 0** na região testada. Isso não prova cobertura de todos os setores desses mapas.

## O que foi alterado

- `AssetAudit.cpp`: scanner passivo para arquivos `.obj` formato `HF Object File!` versão 600. Lê setores, índices e registros de 20 bytes como em `SceneWorld::open/build`, respeita o limite do renderizador de 25 registros por setor; ignora entradas que não são modelos (dois bits superiores do `typeId`).
- Cruza o ID de 14 bits do `.obj` com `sceneobjinfo.txt`. Identifica nome `.lmo`, tipo de objeto (visual/auxiliar), quantas instâncias o usam e em qual mapa.
- `scene_used`: categoria NOVA no JSON/CSV; `status=missing` significa que um modelo referenciado por um `.obj` analisado não existe na instalação, ao contrário do campo `scene_catalog`, que descreve TODO o catálogo histórico.
- `scene_unmapped`: categoria NOVA para IDs existentes nos `.obj` mas sem nome de modelo válido na tabela.
- `scene_catalog`: o campo `detail` passa a indicar se o modelo foi referenciado nos `.obj` analisados ou não. Quando não há `.obj` a análise de uso é **indeterminada**, nunca "não utilizado".
- `summary` preserva o schema legado `top-assets-05-af-v1` e acrescenta, de forma compatível, `scene_maps_scanned`, `scene_instances`, `scene_models_used`, `scene_models_missing_used`, `scene_models_case_mismatch_used`, `scene_unresolved_instances`, `scene_catalog_not_referenced_in_scanned_maps`, `scene_invalid_sections`, `scene_records_ignored_by_renderer_cap`.
- `case_mismatches` existente não é inflado pela repetição de um modelo em vários mapas. Os novos mismatches têm contador separado.
- `UI05AHTests.cpp`: fixture binária de `.obj` com IDs válidos, faltantes, helpers, registros ignorados, nomes em caixa diferente, `.obj` ausente e setor inválido.
- `main.cpp`: banner atualizado para identificação da versão.

**Nenhuma regra de jogo, rendering, animação, save ou dado histórico do usuário foi alterado.** A auditoria é leitura apenas, grava somente o JSON e CSV escolhidos.

## Resultados neste ambiente

Compilação headless de `top_asset_audit` e de `top_regression_tests`: **48/48 testes aprovados**, incluindo todos os testes da versão anterior (47) e o teste novo. Para permitir o teste histórico de Play, a estrutura `Client/scripts/` foi extraída do pacote de scripts fornecido; nenhum arquivo `.obj` ou de modelos reais foi fornecido aqui. A compilação e execução integral do `top_gl` com GLX não foi verificada neste ambiente.

## Como testar na instalação original do Ubuntu

```bash
cmake -S sources -B build -DTOP_BUILD_UI_TESTS=ON
cmake --build build --target top_asset_audit top_gl top_regression_tests -j4
ctest --test-dir build --output-on-failure

./build/top_asset_audit --client Client --json 05AH_assets.json --csv 05AH_assets.csv
```

Leitura de resultados: no `05AH_assets.csv`, filtre **`category == scene_used` e `status == missing`** para encontrar o nome dos modelos concretamente referenciados mas ausentes; a coluna `map` identifica o mapa e `detail` inclui o total de instâncias. Para identificar problemas de tabela, filtre `category == scene_unmapped`. O sumário de `scene_models_missing_used` conta modelos distintos globalmente, não instâncias e não inclui modelos que aparecem apenas no catálogo.

Para checar apenas um mapa:

```bash
./build/top_asset_audit --client Client --map dreamisland --json dreamisland_AH.json --csv dreamisland_AH.csv
```

Não use `--strict` para inferir falha de renderização: ele ainda verifica os critérios amplos da 05-AF (`binglang2.map` continuará faltando nessa instalação). Validade estrutural de `.lmo` e equivalência visual DX9/GL não são comprovadas por este scanner.

## Próxima etapa

Depois de obter o relatório **05-AH gerado com o Client completo**, investigar apenas os `scene_used missing`, `scene_unmapped` e `scene_invalid_sections`, em ordem de impacto; depois voltar às funcionalidades históricas offline que ainda faltam, sobretudo missões/NPCs/habilidades. Não refazer o alinhamento das espadas: o usuário confirmou que esse problema foi resolvido.
