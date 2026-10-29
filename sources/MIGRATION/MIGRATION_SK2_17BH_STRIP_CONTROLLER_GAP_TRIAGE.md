# SK2.17-BH — triagem atual da lacuna STRIP e owners

## Resultado medido

Reexecutei `build/top_fx_coverage_audit --client Client --out build/sk217bg-fx-audit`. O relatório atual contém 1.031 PAR, 1.227 EFF, 3.476 sistemas, 2.832 partes, 25 gaps, 307 skills com gaps, zero drift e zero falhas estruturais. `type:6` permanece no topo com 1.041 sistemas/504 arquivos/242 skills estáticos.

Esse número agrega comportamentos diferentes. `feature:strip_nested_eff_frame_step` registra 875 sistemas em 500 PARs/242 skills e já tem clocks de quadro/grupo (`TOP_HISTORICAL_STRIP_EFF_FRAME_STEP`) e UV/textura (`TOP_HISTORICAL_STRIP_EFF_UV_CLOCK`) opt-in. O DX9 cria um único slot e `_FrameMoveStrip` atualiza atributos da primeira chave e avança o efeito filho (`arquivo/fontes/Engine/src/MPParticleSys.cpp:3173-3230`). O GL resolve filhos `.eff` em `EffectSystem.cpp:661-730` e ativa os clocks somente nos ramos type 6 em `EffectSystem.cpp:2503-2510`. O ranking de tipo não mede ausência de todas essas subfeatures.

## Original DX9 × porte GL × decisão

| Original DX9 | Porte GL atual | Resultado da triagem |
|---|---|---|
| `CEffectObj::SetEffect` chama `SetStripCharacter` para `NODE_CHA` e `SetStripItem` para `NODE_ITEM`, se o controlador possui strips (`arquivo/fontes/Client/src/EffectObj.cpp:1048-1060`). `CMPPartCtrl` encaminha o owner aos objetos CMPStrip (`arquivo/fontes/Engine/src/MPParticleCtrl.cpp:326-340`). `MPEffectCtrl` também tem setters item com `loop=true` (`arquivo/fontes/Engine/src/MPEffectCtrl.cpp:313,419`; o default do setter é false em `MPParticleCtrl.h:331`). | O renderizador consulta pares de matrizes somente para Cast do ator/item do jogador (`sources/Engine/src/top/EffectSystem.cpp:1676-1686`). A origem dessas matrizes é o corpo/equipamento de `ActorSkin` (`sources/Engine/src/top/ActorSkin.cpp:1408-1468`; atualização no app em `sources/Platform/linux/main.cpp:2842-2844`). | Há suporte comprovado para owners do jogador. Os fontes não provam um caminho GL para owners genéricos nem execução DX9 por asset. Sem um evento com identidade e dummies de outro owner, não acrescento um provedor hipotético. |
| O audit atual decodificou 6 trailers em 6 PARs; zero falhas. Todos aparecem no catálogo `sceneffectinfo`; cinco PARs têm linha no grafo de dependências (16 IDs de skill estáticos no total), e um trailer não tem essa linha. O inventário de dependências não prova execução. `xzsb1.par`/`xzsb2.par` usam dummies 3/6 e skill 266; `mosterlight.par` usa dummies 9/15 e a dependência estática aponta skill 238, “Colossus Smash” (`Client/scripts/table/skillinfo.txt:184`; `dependencies.csv:338`). `swordlight02.par` é canal item e liga-se estaticamente a 12 skill IDs. | O audit registra os setters GL do jogador, mas marca runtime como não provado. | Não tratar `mosterlight` como prova de owner monstro: o nome do arquivo não identifica quem executa o evento. A relação skill/asset é evidência de catálogo, não de execução numa sessão. |
| `01000012.par` tem canais estáticos `impact_ground;item` e skills 10/111; `01030010.par` tem uma linha de catálogo sem linha de dependência skill. | O caminho GL de direct strips lida com Cast de ator/item; não foi localizado um owner runtime para o canal de superfície nem para o recurso sem dependência. | Manter os dois casos como desconhecidos. Não associar dummies de jogador a impacto de terreno e não remover recursos. |

## `.eff` recusados e utilidade

Na auditoria FX desta execução, 31 `.eff` falham no modo estrito por parâmetros de modelo não finitos; os 31 decodificam no modo diagnóstico, e `fx_decode_failures.csv` tem somente o cabeçalho. Trinta têm referência direta de PAR e as 31 linhas registram `dx9_catalog_load_evidence=enumerated_loaded_indexed`. `effect/fboomerff.eff` não tem essa aresta, mas tem PAR irmão/catalogação DX9; `runtime_use_proven=no`. Nenhum foi classificado como morto. Ao comparar filhos `.eff` que incluem esses arquivos, habilite `TOP_HISTORICAL_EFF_RAW_MODEL_PARAMS=1`, já existente e desligado por padrão.

## Fatos, inferência e desconhecido

- **FATO:** os callsites DX9 de actor/item e o caminho GL de player body/equipment estão citados acima; o audit de anchors encontrou 6/6 trailers, 6/6 catalogados, zero decode failures e uma linha sem dependência.
- **INFERÊNCIA:** `type:6` no topo representa exposição agregada dos filhos `.eff` e direct strips, não um controlador único totalmente ausente.
- **DESCONHECIDO:** quais recursos chegam de fato a `CEffectObj::SetEffect`, o owner concreto por execução, se `mosterlight.par` aparece em ação de jogador ou monstro, quem possui o strip `impact_ground`, se algum dos assets passa pelos callsites `MPEffectCtrl` com `loop=true` em runtime, cadência de runtime e paridade visual. O source audit não responde essas perguntas.

## Execuções desta triagem

- `build/top_fx_coverage_audit --client Client --out build/sk217bg-fx-audit` — exit 0.
- `build/top_direct_strip_anchor_audit --client Client --out build/sk217bg-direct-strip-audit --deps sources/auditoria-historica/dependencies.csv --scene-effects scripts/table/sceneffectinfo.txt --dx9-model-eff arquivo/fontes/Engine/src/MPModelEff.cpp --dx9-particle-ctrl arquivo/fontes/Engine/src/MPParticleCtrl.cpp --dx9-effect-ctrl arquivo/fontes/Engine/src/MPEffectCtrl.cpp --dx9-client-effect-obj arquivo/fontes/Client/src/EffectObj.cpp --gl-effect-system sources/Engine/src/top/EffectSystem.cpp --gl-actor-skin sources/Engine/src/top/ActorSkin.cpp` — exit 0, 1.031 PAR, 6 trailers, zero decode failures.
- CSVs/JSON: `build/sk217bg-fx-audit/` e `build/sk217bg-direct-strip-audit/`.
- Não alterei C++ nem rodei build/CTest nesta etapa de triagem. A última build Debug completa após SK2.17-BG incluiu `top_gl`; CTest passou 138/138. Sem comparação visual DX9/OpenGL.

## Próximo passo que depende de evidência externa

Para decidir se é preciso ampliar o owner do direct CMPStrip, é necessário um trace/captura DX9 que mostre um desses seis recursos durante uma execução e identifique o owner e os dummies lidos. Até existir essa evidência, o código permanece no escopo comprovado do player; a comparação visual das opções STRIP continua pendente.
