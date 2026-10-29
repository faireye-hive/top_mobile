# SK2.17-AX — catálogo de scene effects no audit CMPStrip

## Gap e evidência

O audit AW relatava `01030010.par` como trailer direto sem linha em `dependencies.csv`. A busca no Client encontrou o arquivo em `Client/scripts/table/sceneffectinfo.txt`, ID 217, nome “Sea Wave”. A tabela é carregada pelo cliente DX9 (`arquivo/fontes/Client/src/GameAppInterface.cpp:1359`); `CGameScene::AddSceneEffect(id)` chama `CEffectObj::Create(id)` (`arquivo/fontes/Client/src/SceneCreateNode.cpp:325-341`; implementação do lookup em `arquivo/fontes/Client/src/EffectObj.cpp:669-710`). O mapa pode instanciar esse ID pelo caminho de scene effects (`arquivo/fontes/Client/src/GameAppMsg.cpp:130-135`). Isso comprova catálogo e caminho de criação, mas não comprova que o ID 217 seja executado por um mapa ou sessão.

## Original DX9 x porte GL x audit

| Assunto | DX9 | GL | Resultado AX |
|---|---|---|---|
| Catálogo | Carrega `sceneffectinfo` e resolve IDs para `CEffectObj`. | O audit de CMPStrip cruza assets e providers GL, mas antes usava apenas `dependencies.csv`, voltado a relações de skills. | O auditor agora lê a tabela Client e exporta `scene_effect_ids`, nomes e flag de catalogação por trailer. |
| `01030010.par` | Entrada explícita ID 217, “Sea Wave”; existe caminho `AddSceneEffect(217)` no carregamento de objetos de cena. | Nenhum vínculo de runtime/mapa foi demonstrado nesta inspeção. | `scene_effect_catalogued_runtime_unproven`; não é classificado como sem uso nem como executado. |
| Outros trailers | IDs de cena também existem para os seis trailers encontrados. | A geometria de dono continua condicionada às matrizes do player no GL. | Os seis trailers têm registros em `sceneffectinfo`; o CSV de dependências de skills continua com uma lacuna. |

## Resultado medido

Client: 1.031 `.par`, seis trailers em seis arquivos, seis registros presentes em `sceneffectinfo`, 16 IDs de skill, zero falhas de parsing/decodificação e um arquivo sem referência no catálogo de dependências de skills. O JSON adiciona `scene_effect_catalog_loaded` e `scene_effect_catalogued_records`; o CSV mostra o ID/nome da tabela por trailer. Catalogação não é prova de uso em runtime.

## Implementação e validação

Esta etapa só altera o auditor e sua fixture; não altera o renderer nem os recursos. O teste sintético cobre um `.par` sem dependência de skill mas presente no catálogo scene-effect. Alvos `top_direct_strip_anchor_audit` e `top_direct_strip_anchor_audit_tests` compilaram em Debug; o fixture focal passou, CTest completo passou **136/136**, e o fixture passou sob ASan/UBSan (`ASAN_OPTIONS=detect_leaks=0`; LSan não executado). Sem comparação visual DX9/GL.

```bash
cmake --build build --target top_direct_strip_anchor_audit top_direct_strip_anchor_audit_tests -j$(nproc)
ctest --test-dir build -R ui05sk217v_direct_strip_anchor_audit --output-on-failure
build/top_direct_strip_anchor_audit \
  --client Client --out build/sk217ax-direct-strip-audit \
  --deps sources/auditoria-historica/dependencies.csv \
  --scene-effects scripts/table/sceneffectinfo.txt \
  --dx9-model-eff arquivo/fontes/Engine/src/MPModelEff.cpp \
  --dx9-particle-ctrl arquivo/fontes/Engine/src/MPParticleCtrl.cpp \
  --dx9-effect-ctrl arquivo/fontes/Engine/src/MPEffectCtrl.cpp \
  --gl-effect-system sources/Engine/src/top/EffectSystem.cpp \
  --gl-actor-skin sources/Engine/src/top/ActorSkin.cpp
```

Saída: `build/sk217ax-direct-strip-audit/`. Patch incremental: `SK217AX.patch`.
