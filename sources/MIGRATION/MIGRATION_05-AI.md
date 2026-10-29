# Tales of Pirates offline — Migracao 05-AI: prioridade real dos modelos ausentes

Base: **05-AH**, validada no Ubuntu do usuario com 48/48 testes e `top_gl` desenhando Argent City. **Nao modifica as armas**, cuja fixacao nas maos ja foi corrigida em migracoes anteriores.

## Diagnostico real recebido do usuario (05AH_assets.json)

- 53 mapas identificados, 52 arquivos `.map` validos, 47 `.obj` analisados.
- 266.852 instancias de objetos nesses arquivos; 628 modelos distintos referenciados.
- 78 modelos distintos usados mas ausentes, totalizando **1.944 instancias visuais** em **8 mapas**. Nenhuma dessas instancias foi classificada como auxiliar.
- 32 instancias sem ID resolvido: 29 usam ID 733 em `l4map`, 3 usam ID 409 em `ctf`.
- 554 referencias no catalogo nao aparecem nos `.obj` analisados; isso NAO prova que sejam desnecessarias para outras versoes ou conteudos.
- Modelos ausentes prioritarios por numero de instancias: `starwalkway` 544; `kyjj_3` 358; `tribulation` 358; `kyjj_2` 241; `l4map` 205; `kyjj_1` 203; `room` 19; `ctf` 16.
- `starwalkway` usa `model/scene/sw_ta.lmo` em 531 das 544 instancias afetadas.
- O cliente nao possui os bytes desses 78 arquivos nos materiais fornecidos. Nenhuma geometria foi inventada.

## Codigo adicionado

- `AssetAudit.h`: nova estrutura `SceneMapImpact`, dados por mapa e contadores globais `scene_missing_instances` e `scene_maps_with_missing_models`.
- `AssetAudit.cpp`: consolida o impacto DURANTE a leitura real dos `.obj`, sem contar objetos duas vezes. A informacao e acrescentada ao JSON legado de forma compativel.
- `--scene-impact CSV`: exporta os mapas com `.obj` analisado, ranqueados por instancias ausentes, com total de objetos, numero de modelos, impacto visual e percentual. Mapas sem `.obj` ficam fora da lista, pois nao foram inspecionados.
- `--missing-models TXT`: exporta lista UNICA de caminhos relativos ao `Client/`, baseada apenas em `scene_used` com status `missing`. Nao inclui os centenas de modelos apenas catalogados.
- `UI05AITests.cpp`: testa contagem, categoria visual/auxiliar, 32-bit reader original indireto, relatorio por mapa, deduplicacao, classificacao de arquivos catalogados nao usados e ordenacao do ranking.
- `main.cpp`: atualiza banner de versao.

### Exemplo de uso

```bash
cmake -S sources -B build -DTOP_BUILD_UI_TESTS=ON
cmake --build build --target top_gl top_asset_audit top_regression_tests -j4
ctest --test-dir build --output-on-failure

./build/top_asset_audit \
  --client Client \
  --json 05AI_assets.json \
  --csv 05AI_assets.csv \
  --scene-impact 05AI_impacto_mapas.csv \
  --missing-models 05AI_modelos_ausentes.txt
```

`05AI_impacto_mapas.csv` ordena os mapas por `missing_instances`. O percentual usa apenas instancias classificadas como `scene_used missing`; IDs desconhecidos ficam em coluna separada, sem inferir modelo. `05AI_modelos_ausentes.txt` tem exatamente um caminho por `.lmo` ausente; serve como manifesto para procurar a versao correta de `Client/`. Nenhum arquivo original e criado, apagado ou renomeado pela auditoria.

### Validacao

- CMake/testes **49/49 aprovados** neste ambiente com os scripts historicos de teste.
- `top_asset_audit` compilou headless, sem depender de GLX.
- A vinculacao/execucao de `top_gl` ainda depende de validacao no Ubuntu com os headers OpenGL. A 05-AH anterior ja havia aberto e desenhado o primeiro quadro no Ubuntu do usuario.
- JSON e CSV gerados pelo usuario permitem reproduzir os 1.944 impactos e os 78 nomes sem extrair novamente a pasta `Client/`.

## Limites e proximos passos

Esses relatorios **nao corrigem a ausencia fisica dos modelos**. Recuperar os assets requer encontrar os `.lmo` autenticos da mesma edicao/versao do cliente, comparar seus IDs e carregar os recursos com o formato historico. Nao usar modelos arbitrarios so para zerar contadores. Manter em paralelo o foco nas funcionalidades offline de NPCs, missoes e habilidades, sem introduzir servidor nem multiplayer.
