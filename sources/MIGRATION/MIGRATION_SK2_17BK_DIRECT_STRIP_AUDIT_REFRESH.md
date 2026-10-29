# SK2.17-BK — reexecução atual do audit CMPStrip direto

## Motivo

O CSV AX disponível dizia `dx9_character_setter_callsite_observed=false` e classificava os trailers actor como se não houvesse callsite. O fonte atual do auditor lê `EffectObj.cpp`, e o original contém `SetStripCharacter` na linha 1055. Recompilei o alvo e executei uma nova auditoria com `--dx9-client-effect-obj` explícito para substituir essa conclusão por evidência observada.

## DX9 original × porte OpenGL × resultado atualizado

| Original DX9 | Porte GL atual | Resultado BK |
|---|---|---|
| `CEffectObj::SetEffect` chama `SetStripCharacter` para `NODE_CHA` e `SetStripItem` para `NODE_ITEM` quando existe CMPStrip (`arquivo/fontes/Client/src/EffectObj.cpp:1048-1060`). `CMPStrip::Play` consome os dummies do item ou do personagem (`arquivo/fontes/Engine/src/MPModelEff.cpp:1251-1272`). | O desenho de `directStrips` ocorre somente em Cast com `playerActorCast` ou `playerItemCast` (`sources/Engine/src/top/EffectSystem.cpp:1676-1686`); os provedores são os dummies do corpo/item do jogador (`sources/Engine/src/top/ActorSkin.cpp:1408-1468`). | O audit recém-executado encontrou o callsite actor no DX9, mas registra a execução por cada recurso como não comprovada. Não há prova de matriz de NPC/monstro no GL. |

## Resultado Client

Auditoria em 1.031 PARs: seis trailers em seis arquivos, 16 IDs únicos de skill estáticos, seis entradas em `sceneffectinfo.txt`, zero falhas de decodificação e um arquivo sem referência no CSV de dependências de skill.

- `xzsb1.par`, `xzsb2.par` e `mosterlight.par`: canal actor; callsite `SetStripCharacter` existe, uso runtime não comprovado.
- `swordlight02.par`: canal item; callsite existe, uso runtime não comprovado.
- `01000012.par`: canais item e `impact_ground`; callsite de item existe, owner para a superfície não comprovado.
- `01030010.par`: trailer catalogado como scene effect ID 217, sem referência no grafo de dependências examinado; execução continua desconhecida.

O resultado antigo de `build/sk217ax-direct-strip-audit/` permanece contraditório e não é a saída de referência desta revisão. O novo CSV/JSON foi gravado em `build/sk217bk-direct-strip-audit/`. O motivo exato da divergência do artefato AX não ficou estabelecido; a reexecução BK passou o caminho explicitamente e observou `dx9_character_setter_callsite_observed=true`, com linha `1055` por trailer.

## Fatos, inferências e desconhecidos

- **FATO:** callsites condicionais estão no código DX9; os provedores GL observados são específicos do jogador.
- **INFERÊNCIA:** canais actor são candidatos a usar um personagem proprietário, mas a relação estática do PAR/skill não diz qual personagem é passado em runtime.
- **DESCONHECIDO:** se cada uma das 16 skills executa o trailer; qual entidade fornece os dummies em cada execução; uso runtime do arquivo sem dependência; paridade visual DX9/OpenGL.

Não alterei o renderer nem conectei dummies do jogador como substitutos para outros atores. Fazer isso sem identificar o owner DX9 poderia desenhar o trailer no personagem errado.

## Teste e reprodução

`top_direct_strip_anchor_audit` e `top_direct_strip_anchor_audit_tests` compilaram em Debug; CTest focal passou 1/1. A fixture do auditor já verifica detecção do callsite actor e item, incluindo as chaves JSON correspondentes. Não houve mudança de runtime nem comparação visual.

```bash
cmake --build build --target top_direct_strip_anchor_audit top_direct_strip_anchor_audit_tests -j$(nproc)
ctest --test-dir build -R ui05sk217v_direct_strip_anchor_audit --output-on-failure
build/top_direct_strip_anchor_audit \
  --client Client \
  --out build/sk217bk-direct-strip-audit \
  --deps sources/auditoria-historica/dependencies.csv \
  --scene-effects scripts/table/sceneffectinfo.txt \
  --dx9-model-eff arquivo/fontes/Engine/src/MPModelEff.cpp \
  --dx9-particle-ctrl arquivo/fontes/Engine/src/MPParticleCtrl.cpp \
  --dx9-effect-ctrl arquivo/fontes/Engine/src/MPEffectCtrl.cpp \
  --dx9-client-effect-obj arquivo/fontes/Client/src/EffectObj.cpp \
  --gl-effect-system sources/Engine/src/top/EffectSystem.cpp \
  --gl-actor-skin sources/Engine/src/top/ActorSkin.cpp
```

Esta etapa atualiza evidência, não o suporte runtime. O suporte STRIP continua parcial e não validado visualmente. Patch incremental: `SK217BK.patch`.
