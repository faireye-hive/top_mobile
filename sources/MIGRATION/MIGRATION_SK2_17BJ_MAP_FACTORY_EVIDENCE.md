# SK2.17-BJ — evidência DX9 para IDs de efeitos colocados em mapas

## Objetivo e medição

O audit SK2.17-AZ encontrou 10 colocações de mapa sem entrada explícita em `sceneffectinfo.txt`, IDs 21 (6 colocações) e 22 (4). A ausência de uma linha no catálogo, sozinha, não prova se o factory DX9 termina sem uma instância válida. Esta etapa estende a auditoria para verificar a cadeia de chamada DX9 e reportar o resultado como desconhecido quando a evidência não basta. Nenhum comportamento runtime foi alterado.

## Comparação: DX9 original × OpenGL × audit

| Original DX9 | Porte GL atual | Auditoria BJ |
|---|---|---|
| Para registros tipo 1, `GameAppMsg` chama `AddSceneEffect(GetID())` (`arquivo/fontes/Client/src/GameAppMsg.cpp:128-136`). `AddSceneEffect` pede `Create(id)` e valida o efeito somente se `Create` retornar sucesso (`SceneCreateNode.cpp:325-338`). `CMagicEff::Create` chama `GetMagicInfo(id)` e verifica o ponteiro (`EffectObj.cpp:669-710`); o alias consulta `CMagicSet` (`EffectSet.h:104-106,239-242`), carregada de `sceneffectinfo.txt` (`GameAppInterface.cpp:1355-1359`). | A cena extrai registros `kind==1` quando o switch está ativo (`SceneWorld.cpp:474-488`). `setMapSceneEffects` ignora IDs sem definição/caminho PAR (`EffectSystem.cpp:624-641`). | O audit agora cruza o ID do mapa, a definição catalogada, a chamada DX9, o carregamento da tabela e o guard de `Create`, registrando evidência por ID (`SceneEffectMapAudit.h:23-32,122-138,217-240`). |

### Fatos, inferências e desconhecidos

- **FATO:** o Client contém 47 mapas válidos, 10.770 colocações tipo 1 e 21 IDs; 10.760 colocações têm entrada de catálogo. IDs 21 e 22 somam as 10 sem entrada.
- **FATO:** os arquivos de origem necessários para verificar chamada, alias, carregamento do catálogo e guard de `Create` foram encontrados e examinados; o audit reporta `dx9_scene_effect_factory_evidence_complete=true`.
- **FATO:** `GetRawDataInfo(id)` retorna `nullptr` se o ID estiver fora do intervalo válido; para ID válido, devolve o slot pré-alocado (`arquivo/fontes/Engine/include/MPResData.h:164-173`). Portanto, “não há linha no catálogo” não implica necessariamente que o ponteiro seja nulo.
- **INFERÊNCIA:** os IDs sem definição provavelmente não produzem os controladores catalogados pelo caminho comum; a definição de `CMagicInfo` vazia e a resolução do nome de PAR são relevantes para o resultado.
- **DESCONHECIDO:** se os IDs 21/22 terminam sem instância DX9 válida em todas as configurações. O guard de ponteiro não basta para concluir isso; não foi feita execução do cliente DX9 que capture o retorno do factory. O porte segue pulando IDs sem definição para não inventar um recurso.

## Mudança

O `top_scene_effect_map_audit` ganhou argumentos para os arquivos de factory e catálogo DX9. A coluna `dx9_factory_outcome` distingue `dx9_catalog_record_present`, `dx9_catalog_record_absent_factory_result_unproven` e `dx9_factory_source_unverified`; a coluna de evidência indica a cadeia estática verificada. O JSON traz a contagem de IDs sem catálogo cujo resultado de factory continua não comprovado. Não houve mudança gráfica nem novo switch.

## Resultado Client e arquivos

Comando executado:

```bash
build/top_scene_effect_map_audit \
  --client Client \
  --out build/sk217bj-map-factory-audit \
  --dx9-game-app-msg arquivo/fontes/Client/src/GameAppMsg.cpp \
  --dx9-scene-obj-file arquivo/fontes/Client/src/SceneObjFile.cpp \
  --dx9-effect-obj arquivo/fontes/Client/src/EffectObj.cpp \
  --dx9-effect-set arquivo/fontes/Client/src/EffectSet.h \
  --dx9-table-loader arquivo/fontes/Client/src/GameAppInterface.cpp \
  --gl-scene-world sources/Engine/src/top/SceneWorld.cpp
```

Saída observada: 47 mapas válidos; 10.770 colocações; 10.760 catalogadas; 10 sem catálogo; 19 PARs catalogados e decodificados; 33 sistemas; zero mapas malformados. O resumo lista dois IDs sem catálogo e mantém seu resultado DX9 como não comprovado. CSVs/JSON em `build/sk217bj-map-factory-audit/`.

## `.eff` com recusa estrita

A auditoria Client mais recente disponível registra 31 recusas do modo estrito, todas recuperáveis pelo parser diagnóstico com parâmetros brutos de modelo. Trinta têm referências PAR diretas. `fboomerff.eff` não tem referência direta; o catálogo relaciona o PAR irmão `fboomerff.par` ao ID 919/skill 308, mas uso runtime permanece desconhecido. Nenhum foi classificado como inútil nem removido. O modo de parâmetros brutos segue atrás de `TOP_HISTORICAL_EFF_RAW_MODEL_PARAMS=1`.

## Testes e limites

O teste fixture `ui05sk217ay_scene_effect_map_audit` passou (1/1). Ele verifica uma definição catalogada e uma sem definição, a cadeia DX9 completa e o caso de fonte incompleta, que deve produzir `dx9_factory_source_unverified`. Os alvos do auditor e fixture compilaram em Debug; o CTest completo passou **139/139**. O fixture ASan/UBSan também terminou com código 0 (`ASAN_OPTIONS=detect_leaks=0`, `UBSAN_OPTIONS=halt_on_error=1`). A auditoria lê dados estáticos; não prova execução ou aparência no cliente.

Patch incremental: `SK217BJ.patch`. Como esta etapa altera somente a auditoria, reverta com:

```bash
git apply -R SK217BJ.patch
```
