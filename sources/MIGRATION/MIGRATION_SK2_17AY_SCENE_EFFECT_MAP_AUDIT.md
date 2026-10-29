# SK2.17-AY — audit de efeitos colocados em mapas

## Resultado

Foi adicionado o utilitário somente-leitura `top_scene_effect_map_audit`. Ele percorre os `.obj` de `Client/map`, extrai registros de tipo 1 e os cruza com `Client/scripts/table/sceneffectinfo.txt`. Também registra evidência estática do ramo DX9 e do filtro atual em `SceneWorld`. O utilitário não altera o runtime nem declara equivalência visual.

## Evidência: DX9 original x porte OpenGL x resultado

| Assunto | Original DX9 | Porte atual | Mudança desta etapa |
|---|---|---|---|
| Registro de mapa | `SSceneObjInfo` separa tipo e ID nos dois bits altos; os campos são `nX`, `nY`, altura, yaw e escala (`arquivo/fontes/Client/src/SceneObjFile.h:15-31`). `ReadSectionObjInfo` lê os registros e soma a origem da seção às coordenadas (`arquivo/fontes/Client/src/SceneObjFile.cpp:477-493`). | O parser da cena usa stride de 20 bytes e interpreta bits de tipo/ID. | Audit lê somente os `.obj` versão 600 e soma origem de seção com a mesma escala de 100 observada no DX9. |
| Criação do efeito | Para tipo 1, `GameAppMsg` chama `AddSceneEffect(id)` e define validade, altura, posição e yaw (`arquivo/fontes/Client/src/GameAppMsg.cpp:128-136`). O criador aloca efeito, chama `Create`, faz `Emission` na origem e marca válido (`arquivo/fontes/Client/src/SceneCreateNode.cpp:325-338`). | `SceneWorld::build` continua após qualquer `kind != 0`, então os efeitos do `.obj` não são instanciados por esse caminho (`sources/Engine/src/top/SceneWorld.cpp:465-470`). | Audit registra a diferença; nenhum comportamento foi habilitado. |
| Client medido | 47 arquivos `.obj` versão 600 válidos; sem seção malformada ou acima do limite DX9 de 25 registros. | Não há consumidor GL desses registros tipo 1 no trecho examinado. | 10.770 colocações, 21 IDs distintos; 10.760 IDs catalogados. ID 21: 6 colocações sem entrada de catálogo; ID 22: 4. |

## Fatos, inferências e desconhecidos

**Fatos medidos:** o novo audit emitiu `maps=47 valid=47 placements=10770 ids=21 catalogued=10760 uncatalogued=10 malformed=0`. Os 10 sem catálogo são 6 registros ID 21 e 4 registros ID 22. O audit também não encontrou seções que excedam `MAX_MAP_SECTION_OBJ=25`.

**Fato sobre o escopo do DX9:** o leitor e o ramo de criação estão presentes nas fontes citadas. `AddSceneEffect` chama `CEffectObj::Create`, depois `Emission`; `GameAppMsg` aplica posição, altura e yaw. Isso prova o caminho de carregamento/criação do objeto de cena, mas não prova que cada `.par` percorre sem falha todos os controladores ou que foi visto em execução.

**Ciclo observado no original:** `SceneFrameMove` chama `FrameMove` para cada efeito válido (`arquivo/fontes/Client/src/SceneFrameMove.cpp:119-130`); `CMagicEff::FrameMove` atualiza o controlador de partículas (`EffectObj.cpp:1088-1138`). Na renderização, o objeto é invalidado quando o `PartCtrl` deixa de tocar (`EffectObj.cpp:1142-1207`). `SetInvalidByTime` exclui IDs 1–99 do timeout (`EffectObj.cpp:1417-1439`), e `ClearSceneEffect` invalida os efeitos que `IsSceneEffect` classifica como cena na limpeza/troca de mapa (`Scene.cpp:1771-1779`). Esses fatos orientam a futura implementação de duração; não provam loop contínuo, pois `Play`/`SetPlayType` e os parâmetros de cada `.par` também determinam o término.

**Inferência:** como o GL ignora os registros tipo 1, efeitos estáticos de mapa são uma lacuna de cobertura no caminho atual do `SceneWorld`. A contagem é de colocações em arquivos do Client, não de efeitos simultaneamente ativos.

**Desconhecido:** IDs 21 e 22 não têm definição explícita em `sceneffectinfo.txt`; falta verificar como o carregador de tabela DX9 trata slots sem linha e se são conteúdo legado. O ciclo básico de criação/atualização/encerramento foi localizado, mas falta mapear a semântica de `Play`/`SetPlayType`, culling/visibilidade e todos os tipos de controlador no caminho de mapa antes de implementar a criação em GL. Nada nesta etapa sugere eventos de colisão ou impactos.

## `.eff` que falham no decodificador estrito

O relatório anterior `build/sk217av-current-audit/fx_eff_usefulness.csv` foi recontado: 31 `.eff` falham no parser estrito e os 31 são recuperados pelo decodificador diagnóstico com parâmetros brutos de modelo. Todos os 31 são enumerados/carregados/indexados pelo caminho de catálogo DX9 (`arquivo/fontes/Engine/src/MPResManger.cpp:1351-1405`); 30 têm referência direta por `.par`. Portanto, a falha estrita não é motivo para descartá-los.

`effect/fboomerff.eff` é o único sem referência direta. O CSV mostra um `.par` irmão `effect/fboomerff.par`, catálogo de cena ID 919 e dependência de skill 308, além da evidência `enumerated_loaded_indexed`. Isso torna o arquivo potencialmente útil e carregável, mas `runtime_use_proven=no`: a ocorrência em catálogo/dependência não comprova execução. Nenhum dos 31 deve ser eliminado por esse critério.

## Execução

```bash
cmake -S sources -B build -DCMAKE_BUILD_TYPE=Debug -DTOP_BUILD_UI_TESTS=ON
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure

build/top_scene_effect_map_audit \
  --client Client \
  --out build/sk217ay-scene-effect-map-audit \
  --scene-effects scripts/table/sceneffectinfo.txt \
  --dx9-game-app-msg arquivo/fontes/Client/src/GameAppMsg.cpp \
  --dx9-scene-obj-file arquivo/fontes/Client/src/SceneObjFile.cpp \
  --gl-scene-world sources/Engine/src/top/SceneWorld.cpp
```

## Resultado dos testes e limites

- Configuração CMake Debug: executada com sucesso.
- Build completo, incluindo `top_gl`: executado com sucesso.
- CTest completo: **137/137 aprovados**; inclui fixture sintética do `.obj` e do catálogo.
- Audit real: executado nos dados `Client/map` disponíveis, 47/47 arquivos reconhecidos.
- Fixture focal compilada e executada com ASan/UBSan (`detect_leaks=0`): passou. A primeira tentativa em `/tmp` foi bloqueada pelo mount; a execução equivalente em `build/` passou.
- Comparação visual no jogo DX9 e GL: não executada. Não há declaração de fidelidade visual.

O executável depende do formato de mapa versão 600 e da codificação de catálogo lida pelo audit. Os IDs sem catálogo permanecem sem classificação. Esta etapa não modifica Frozen Arrow, Illusion Slash, Taunt, Berserk ou qualquer renderização existente.

## Patch

`SK217AY.patch` contém apenas os arquivos de auditoria, o fixture CMake, este relatório e a atualização do estado. Para desfazer após aplicar: `git apply -R SK217AY.patch`.
