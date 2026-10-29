# SK2.17-AZ — PARs dos efeitos estáticos de mapa

## Resultado

O audit AY foi estendido para abrir, pelo parser estrutural `LegacyParAudit`, o `.par` associado a cada ID de cena catalogado e gerar `scene_effect_map_controllers.csv`. O registro mostra contagem de colocações, versão/controlador PAR, tipo e quantidade de sistemas, modelo e resultado da decodificação. IDs sem catálogo continuam registrados sem inventar uma dependência.

Esta etapa é somente auditoria. Não cria efeitos no runtime.

## DX9 original x porte atual x evidência

| Caminho | DX9 | OpenGL atual | Medição |
|---|---|---|---|
| Resolver catálogo | `CGameApp::InitAllTable` carrega `sceneffectinfo` como `CEffectSet` (`arquivo/fontes/Client/src/GameAppInterface.cpp:1351-1359`); `AddSceneEffect` usa `Create(id)` e `Emission` (`arquivo/fontes/Client/src/SceneCreateNode.cpp:325-338`). | `EffectSystem::open` carrega `SceneEffectCatalog`; `resourceFor` acha o ID e lê o `.par` catalogado (`sources/Engine/src/top/EffectSystem.cpp:413-425,488-505`). | 19 PARs associados aos 19 IDs de mapa catalogados foram encontrados e decodificados; total de 33 sistemas. |
| Loop/atualização | A emissão normal executa `Play(0)` e `SetPlayType(0)` (`arquivo/fontes/Client/src/EffectObj.cpp:1334-1351`). `CMPPartCtrl::SetPlayType(0)` ativa `SetLoop(true)` nos sistemas de partícula e `PLAY_LOOP` nos modelos (`arquivo/fontes/Engine/src/MPParticleCtrl.cpp:392-405`). A cena chama `FrameMove` em cada efeito válido (`arquivo/fontes/Client/src/SceneFrameMove.cpp:119-130`). | A resolução de catálogo está disponível quando um efeito é emitido, mas `SceneWorld::build` pula os registros `kind != 0` (`sources/Engine/src/top/SceneWorld.cpp:465-470`). Main não encaminha colocações tipo 1 ao `EffectSystem`. | Há dados e parser de recurso GL para estes controllers; a criação, loop e atualização a partir das colocações `.obj` não estão ligados ao caminho de mapa. |
| Encerramento | `CMagicEff::Render` invalida quando `PartCtrl::IsPlaying()` é falso (`arquivo/fontes/Client/src/EffectObj.cpp:1142-1207`); `ClearSceneEffect` invalida efeitos de cena na limpeza/troca de mapa (`arquivo/fontes/Client/src/Scene.cpp:1771-1779`). | O `EffectSystem` remove eventos por tempo `expiresAfter`; ainda não tem identidade/ciclo de vida para efeitos estáticos de mapa. | Não há base para reaproveitar diretamente o prazo curto de eventos de skill nos efeitos de mapa. |

## Distribuição dos 10.760 usos catalogados

O tipo e o total abaixo resultam do `sceneffectinfo.txt`, dos `.obj` e da decodificação dos `.par` atuais. Tipo é o `CMPPartSys` gravado no PAR, não uma classificação de fidelidade gráfica.

| Tipo PAR | IDs de cena | Colocações |
|---:|---|---:|
| 1 | 5, 6, 9, 13, 14, 15, 95 | 1.681 |
| 2 | 8, 16, 17, 18 | 1.214 |
| 3 | 3, 4 | 747 |
| 4 | 97 | 1 |
| 6 | 1, 7, 10, 11, 12 | 7.117 |

ID 1 sozinho ocupa 6.771 lugares e usa `wave1.par`, um controller type 6 com modelo `bonew5.eff`. IDs 21 e 22 permanecem sem entrada explícita no `sceneffectinfo.txt`; juntos aparecem em 10 colocações. Não foram classificados como lixo: é necessário examinar como o loader DX9 trata slots de tabela sem linha.

## `.eff` strict failures

O FX audit foi executado novamente no mesmo Client: 1.031 PAR, 1.227 EFF, 3.476 sistemas PAR, 2.832 partes EFF e zero falhas estruturais. Permanecem 31 falhas do decoder EFF estrito; as 31 são recuperadas no caminho diagnóstico e enumeradas/carregadas/indexadas pelo loader de catálogo DX9 (`arquivo/fontes/Engine/src/MPResManger.cpp:1351-1405`). Trinta têm referências PAR diretas; `fboomerff.eff` não tem referência direta, mas possui `fboomerff.par`, ID 919 e skill 308. Runtime não comprovado para esse caso.

Conferi os modelos EFF citados pelos 19 PARs de efeitos de mapa: nenhum desses nomes corresponde às 31 entradas strict-failed desse relatório. Isso não torna os 31 inúteis: 30 continuam referenciados por outros PARs e um tem associação indireta registrada.

## Como reproduzir

```bash
cmake -S sources -B build -DCMAKE_BUILD_TYPE=Debug -DTOP_BUILD_UI_TESTS=ON
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure

build/top_scene_effect_map_audit \
  --client Client \
  --out build/sk217az-scene-effect-controller-audit \
  --scene-effects scripts/table/sceneffectinfo.txt \
  --dx9-game-app-msg arquivo/fontes/Client/src/GameAppMsg.cpp \
  --dx9-scene-obj-file arquivo/fontes/Client/src/SceneObjFile.cpp \
  --gl-scene-world sources/Engine/src/top/SceneWorld.cpp

build/top_par_audit --client Client --out build/scene-effect-par-audit-current
build/top_fx_coverage_audit --client Client --out build/fx-coverage-current \
  --source sources/Engine/src/top/EffectSystem.cpp \
  --dx9-source arquivo/fontes/Engine/src/MPResManger.cpp
```

## Validação e limites

- CMake Debug e build completo, incluindo `top_gl`: executados com sucesso.
- Fixture sintética de `.obj`, catálogo e `.par` mínimo: CTest focal passou; ASan/UBSan passou (`detect_leaks=0`).
- CTest completo: **137/137 aprovados**.
- Audit de mapa: 47/47 arquivos válidos; 19/19 PARs catalogados decodificados; 33 sistemas.
- Audit PAR completo: 1.031/1.031 decodificados, 3.476 sistemas, 110 referências ausentes no Client inteiro.
- FX audit completo: 1.031 PAR, 1.227 EFF, zero falhas estruturais, 31 falhas estritas EFF recuperadas em diagnóstico.
- Comparação visual DX9/GL: não executada; não alego paridade.

A próxima mudança de runtime precisa respeitar a posição, altura e yaw do `.obj`, o loop `Play(0)` e o término/limpeza DX9, além de limitar o custo de 10.760 colocações sem descartar o comportamento original. Esta auditoria não escolhe uma aproximação para esses pontos.
