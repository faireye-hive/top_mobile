# SK2.17-AW — evidência de donos para CMPStrip direto

## Objetivo e escopo

O ranking do FX audit ainda mostra type 6 entre as maiores lacunas: 1.041 sistemas, 504 `.par` e 242 skills. O inventário separado de `CMPStrip` direto mediu seis trailers em seis arquivos e 16 IDs de skill. Esta etapa corrige a classificação do auditor: presença de código genérico de owner não prova que um canal tenha um callsite DX9 nem que seja executado. Nenhum comportamento de desenho foi alterado.

## Original DX9 x porte GL x auditoria

| Assunto | Original DX9 | Porte GL atual | Resultado do audit AW |
|---|---|---|---|
| Dono consumido | `CMPStrip::Play` lê matrizes de `_pItem`; se não houver item, usa `_pCha` (`arquivo/fontes/Engine/src/MPModelEff.cpp:1251-1272`). | O GL só desenha as trails em eventos Cast de player e consulta provedores de matrizes de corpo/item do player (`sources/Engine/src/top/EffectSystem.cpp:1633-1651`; `ActorSkin.cpp:1408-1468`). | A existência dos dois caminhos de implementação não prova que um trailer específico seja invocado por aquele dono. |
| Callsite DX9 de item | `MPEffectCtrl` liga `SetStripItem` quando o modelo do primeiro efeito é item (`arquivo/fontes/Engine/src/MPEffectCtrl.cpp:309-315,415-420`). | O porte fornece matrizes do item equipado do jogador. | O auditor registra o callsite estático, mas classifica uso em runtime como não comprovado. |
| Callsite DX9 de personagem | `CEffectObj` chama `SetStripCharacter` quando o owner é `NODE_CHA` e há CMPStrip (`arquivo/fontes/Client/src/EffectObj.cpp:1048-1056`). | Existe provedor de matrizes do corpo do jogador. | O callsite estático não prova qual owner acompanha cada referência Client nem execução em runtime; matrizes GL de outros personagens não estão estabelecidas. |
| Associação de assets | O DX9 carrega trailers diretos a partir de `.par` com trailer versionado. | O GL decodifica seis trailers Client. | Cinco têm dependências catalogadas; `01030010.par` não tem linha de dependência no CSV. Isso não prova que o asset seja inútil. |

## Fatos, inferências e desconhecidos

- **FATOS desta reexecução:** o audit original desta etapa leu 1.031 `.par`, encontrou 6 trailers em 6 arquivos, 16 IDs únicos, zero falhas de parsing/decodificação e 1 arquivo sem dependência. A inspeção inicial omitiu `Client/src/EffectObj.cpp`; a correção BB confirmou callsites condicionais para personagem e item nesse arquivo. O GL tem provedores de matriz do corpo e item do player e restringe o desenho direto a Cast de player.
- **INFERÊNCIA:** `swordlight02.par` e `01000012.par` têm candidatos a fluxo de item porque aparecem em canais `item` e o DX9 contém a ligação de item. Essa ligação é condicional ao modelo ser item e não comprova que os eventos Client auditados a executem.
- **DESCONHECIDO:** owner real em runtime para canais actor/superfície, uso do arquivo sem dependência, invocação do `SetStripItem` por skill e equivalência visual DX9/GL.

## Resultado atualizado

O CSV registra por trailer os canais e evidências de callsite separadas. Classificações do Client:

- Classificação histórica corrigida em BB: `xzsb1.par`, `xzsb2.par` e `mosterlight.par` têm callsite DX9 para personagem, mas uso em runtime permanece sem prova.
- `item_setter_callsite_present_runtime_unproven`: `swordlight02.par` e canal item de `01000012.par`.
- `surface_channel_owner_runtime_unproven`: canal `impact_ground` de `01000012.par`.
- `dependency_unmatched_owner_unknown`: `01030010.par`.

O JSON produzido pela revisão anterior registrava `dx9_character_setter_callsite_observed=false`; esse valor foi falso-negativo pela omissão de `Client/src/EffectObj.cpp`. O audit BB agora registra o arquivo e a linha do callsite, sem converter evidência estática em prova de execução.

## Correção de evidência — SK2.17-BB

A inspeção desta etapa omitiu `arquivo/fontes/Client/src/EffectObj.cpp`. O original chama `SetStripCharacter` para `NODE_CHA` e `SetStripItem` para `NODE_ITEM` quando o controlador possui CMPStrip (`EffectObj.cpp:1048-1060`). Portanto, a afirmação acima de que não havia callsite DX9 de personagem e a classificação `actor_setter_callsite_missing` eram incorretas. O audit BB inclui esse arquivo e encontrou o callsite de personagem na linha 1055 e o de item na linha 1060. A classificação atual é `actor_setter_callsite_present_runtime_unproven`: a ligação é condicional, e a fonte não prova quais referências do Client executam o caminho. A ausência de matrizes GL genéricas para outros personagens continua uma diferença real; isso não é resolvido por esta correção do audit.

## Teste e repetição

Executado em Debug: `top_direct_strip_anchor_audit` e `top_direct_strip_anchor_audit_tests` compilaram; CTest focal passou **1/1**, a suíte completa passou **136/136**, e o fixture focal passou com ASan/UBSan (`ASAN_OPTIONS=detect_leaks=0`; LSan não executado). Auditoria Client terminou com `par=1031 records=6 files=6 decode_failures=0 no_dependency_row=1`.

```bash
cmake --build build --target top_direct_strip_anchor_audit top_direct_strip_anchor_audit_tests -j$(nproc)
ctest --test-dir build -R ui05sk217v_direct_strip_anchor_audit --output-on-failure
build/top_direct_strip_anchor_audit \
  --client Client --out build/sk217aw-direct-strip-audit \
  --deps sources/auditoria-historica/dependencies.csv \
  --dx9-model-eff arquivo/fontes/Engine/src/MPModelEff.cpp \
  --dx9-particle-ctrl arquivo/fontes/Engine/src/MPParticleCtrl.cpp \
  --dx9-effect-ctrl arquivo/fontes/Engine/src/MPEffectCtrl.cpp \
  --gl-effect-system sources/Engine/src/top/EffectSystem.cpp \
  --gl-actor-skin sources/Engine/src/top/ActorSkin.cpp
```

Não houve mudança no renderer e não houve comparação visual. STRIP continua parcial, sem alegação de equivalência visual. Patch incremental: `SK217AW.patch`.
