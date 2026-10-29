# SK2.17-BB — cobertura do callsite DX9 de CMPStrip direto

## Resultado

Corrigi `top_direct_strip_anchor_audit` para inspecionar também `arquivo/fontes/Client/src/EffectObj.cpp`. A auditoria anterior examinava `MPModelEff.cpp`, `MPParticleCtrl.cpp` e `MPEffectCtrl.cpp`; por isso classificou falsamente os canais `actor` como `actor_setter_callsite_missing`. Esta etapa não altera o renderer nem comportamento do jogo.

## Original DX9 x porte GL x correção

| Original DX9 | Porte GL observado | Resultado/correção BB |
|---|---|---|
| `CEffectObj::SetEffect` verifica `GetStripNum()>0`; para `NODE_CHA`, chama `SetStripCharacter(pCha)`; para `NODE_ITEM`, chama `SetStripItem(pItem)` (`arquivo/fontes/Client/src/EffectObj.cpp:1048-1060`). `CMPPartCtrl` encaminha o owner aos trailers (`arquivo/fontes/Engine/src/MPParticleCtrl.cpp:326-340`). | O GL desenha CMPStrip direto em Casts marcados como ator/item do jogador e obtém matrizes dos dummies do corpo e do item equipado (`sources/Engine/src/top/EffectSystem.cpp:1676-1686`; `sources/Platform/linux/main.cpp:2842-2844`; `sources/Engine/src/top/ActorSkin.cpp:1408-1468`). Não há provedor genérico de outros personagens demonstrado nesses caminhos. | O audit agora recebe o arquivo Client por `--dx9-client-effect-obj`, inclui linhas por arquivo no CSV e expõe evidência separada no JSON. A classificação `actor_setter_callsite_missing` torna-se `actor_setter_callsite_present_runtime_unproven`; não se infere execução para cada skill/asset. |
| `CMPStrip::Play` usa os dummies de `_pItem` ou `_pCha` e não inicia sem owner válido (`arquivo/fontes/Engine/src/MPModelEff.cpp:1251-1272`). | Donos disponíveis são apenas os providers do jogador no caminho examinado; linhas: `EffectSystem.cpp:1676-1686`, `ActorSkin.cpp:1408-1468`. | A auditoria distingue a existência do setter DX9 da disponibilidade de matrizes GL de outros owners. A lacuna de owner não jogador continua em aberto; não foi inventada matriz substituta. |

## Auditoria real do Client

Executado com o auditor atualizado: 1.031 PARs; seis trailers em seis arquivos; zero falhas de decodificação; um trailer sem linha de dependência. Os três trailers nos canais `actor` agora mostram o callsite DX9 de `EffectObj.cpp:1055`. Os canais `item` mostram callsites em `MPEffectCtrl.cpp` e `EffectObj.cpp:1060`. O arquivo sem dependência continua classificado como owner/uso desconhecido, não descartado. CSV/JSON: `build/sk217bb-direct-strip-audit-final/`.

O FX coverage audit reexecutado contra o Client atual encontrou 1.031 PARs, 1.227 EFFs, 3.476 sistemas, 2.832 partes EFF, 26 lacunas, 307 skills com lacunas, zero deriva e zero falhas estruturais. O topo do ranking continua type 6 (1.041 ocorrências/242 skills), seguido por `snow_fire_step` (588/123); a linha não significa que toda subfeature type 6 esteja ausente, pois os caminhos temporais filhos já existem atrás dos switches documentados nas etapas anteriores. A tabela de utilidade contém 31 recusas no modo estrito; as 31 são recuperáveis em diagnóstico, 30 têm referência direta PAR→EFF. O único sem referência direta é `fboomerff.eff`, cujo PAR irmão está catalogado como ID 919/skill 308, e o DX9 o enumera/carrega; execução visual/runtime permanece não provada. Nenhum `.eff` foi descartado. JSON e CSV: `build/sk217bb-fx-audit-final/`.

## Fatos, inferências e desconhecidos

**FATO:** existe callsite DX9 de personagem e item em `EffectObj.cpp:1048-1060`; os callsites são condicionais a owner type e controlador com strips. O CSV/JSON atualizado registra esse arquivo. O porte GL tem providers do corpo e item do jogador, enquanto `EffectSystem.cpp:1676-1686` só processa esses tipos de Cast.

**INFERÊNCIA:** canais `actor` podem obter owner a partir do caminho DX9 de `NODE_CHA`, mas a associação do CSV não identifica o objeto concreto para cada uso.

**DESCONHECIDO:** se cada recurso auditado realmente chega a `CEffectObj::SetEffect` em runtime, qual entidade é o owner e se o GL precisa suportar owners além do jogador para os casos em questão. Não houve comparação visual.

## Testes

- Compilados `top_direct_strip_anchor_audit` e `top_direct_strip_anchor_audit_tests` em Debug.
- CTest focal `ui05sk217v_direct_strip_anchor_audit`: 1/1 aprovado.
- O fixture sintético agora verifica callsite de personagem e item vindos do Client e callsite de item vindo de MPEffectCtrl.
- `SK217BB.patch` passou `git apply --check` e aplicação em cópia isolada pré-BB; os sete arquivos aplicados foram comparados byte a byte com os arquivos entregues.
- Build Debug completo concluiu incluindo `top_gl`; CTest completo: **138/138 aprovados**.
- Fixture focal com ASan/UBSan: **1/1 aprovado**, `ASAN_OPTIONS=detect_leaks=0` e `UBSAN_OPTIONS=halt_on_error=1`. LSan foi desativado; a comparação visual continua pendente.

Repetição:

```bash
cmake -S sources -B build -DCMAKE_BUILD_TYPE=Debug -DTOP_BUILD_UI_TESTS=ON
cmake --build build --target top_direct_strip_anchor_audit top_direct_strip_anchor_audit_tests -j$(nproc)
ctest --test-dir build -R ui05sk217v_direct_strip_anchor_audit --output-on-failure
build/top_direct_strip_anchor_audit \
  --client Client --out build/sk217bb-direct-strip-audit-final \
  --deps sources/auditoria-historica/dependencies.csv \
  --scene-effects scripts/table/sceneffectinfo.txt \
  --dx9-model-eff arquivo/fontes/Engine/src/MPModelEff.cpp \
  --dx9-particle-ctrl arquivo/fontes/Engine/src/MPParticleCtrl.cpp \
  --dx9-effect-ctrl arquivo/fontes/Engine/src/MPEffectCtrl.cpp \
  --dx9-client-effect-obj arquivo/fontes/Client/src/EffectObj.cpp \
  --gl-effect-system sources/Engine/src/top/EffectSystem.cpp \
  --gl-actor-skin sources/Engine/src/top/ActorSkin.cpp
```

O executável do jogo não foi comparado visualmente com o DX9. Relatórios BA/V/AW antigos foram corrigidos para apontar o falso-negativo e a evidência desta etapa.
