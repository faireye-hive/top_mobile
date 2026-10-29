# SK2.18-G — centro de billboard filho vinculado, regra compartilhada

## Resultado e escopo

A correção recupera uma regra do motor para todos os canais elegíveis, sem testes de ID de skill no renderer. Quando um MODEL/STRIP não-billboard vincula um `.eff` billboard ao dummy e `_bModelDir=false`, o DX9 substitui a translação animada do filho pelo centro do vínculo. O GL somava essa posição filha. `TOP_HISTORICAL_BOUND_BILLBOARD_CENTER=1` seleciona a regra corrigida; desligado por padrão.

Taunt foi o caso usado para encontrar a diferença; não é uma exceção codificada. A etapa F já preservou `_bModelDir` no auditor e verificou os billboards filhos; D/E já cobrem Part_trace. Esta etapa não refaz esses mecanismos.

## Original DX9 × GL anterior × mudança

| Condição/comportamento | Original DX9 | Porte antes de G | Mudança G |
|---|---|---|---|
| Flag de formato | `_bModelDir` é lido em PAR v14+ (`arquivo/fontes/Engine/src/MPParticleSys.cpp:4330-4334`). | O loader de runtime pulava esse byte. | `sources/Engine/src/top/EffectSystem.cpp:272-275` preserva o flag em `ParticleProfile::modelDirection` (`sources/Engine/include/top/EffectSystem.h:150`), sem alterar o formato. |
| Vínculo ao dummy | `SelfEffect` segue o dummy se existe, ou a raiz como fallback (`arquivo/fontes/Client/src/Character.cpp:940-952`). `BindingBone` distingue billboard, MODEL/STRIP e `_bModelDir` (`MPParticleSys.cpp:1205-1238`). | Actor/item/target podem receber matrizes de dummy, mas somente a translação entra no centro do efeito. | `hasDummyBinding` verifica os mesmos provedores que posicionam o efeito (`EffectSystem.cpp:1649-1658`). A regra nova só entra quando há matriz efetiva; raiz de fallback não é tratada como dummy vinculado. |
| Desenho do billboard filho | O pai passa a matriz ao filho (`MPParticleSys.cpp:1526-1531`). `CMPModelEff::Render` orienta billboard pela câmera e substitui `_41/_42/_43` pelo vínculo (`arquivo/fontes/Engine/src/MPModelEff.cpp:820-840`). A composição completa do osso em `:845-846` é outro ramo, para filho não-billboard. | O GL só suprimia `keyframe.position` se o **PAR pai** era billboard. Um STRIP não-billboard com filho billboard somava o deslocamento filho mesmo vinculado. | `EffectSystem.cpp:2952-2968` também suprime a posição filha no MODEL/STRIP elegível com dummy real, sob switch. Tamanho, textura e orientação pela câmera seguem seus caminhos existentes. |

## Tradução para OpenGL

O centro do dummy vem de matrizes DX9 row-vector/Z-up e já é convertido pelo bridge para GL X/altura/Z. A regra recuperada usa esse centro mais o offset do pai como origem do quad e ignora a posição filha que o DX9 substitui. Não se aplica rotação completa do osso a esse billboard: o original usa a câmera. A transformação de filhos não-billboard é uma lacuna distinta e não deve ser introduzida por analogia a esse caso.

## Auditoria global, não por skill isolada

Novo `top_effect_binding_audit` usa os leitores existentes de `skillinfo`, `sceneffectinfo`, `.par` e `.eff` para cruzar actor, target, item e item-extra. Não contém lista manual de skills. O CSV registra flags do pai e do filho, dummy configurado, posição filha não zero e estado da resolução. As condições estão confrontadas com o renderer pela mesma fixture sintética usada no teste de integração.

```bash
build/top_effect_binding_audit --client Client --out build/effect-binding-sk218g
```

Execução real: exit 0; 1.880 combinações resolvidas de canal/sistema/subefeito, 22 candidatas, **8 IDs de skill** e 8 PARs distintos candidatos. Há 13 canais não resolvidos literalmente: 7 com caminho PAR ausente (`heallight.pa` ou `01000017.par`) e 6 sem entrada sceneffect correspondente (IDs 221/880). Isso não prova inutilidade: aliases, tabelas e consumidores DX9 precisam ser investigados separadamente. CSV completo: `build/effect-binding-sk218g/bound_billboard_channels.csv`.

| ID | Nome Client | Canal candidato |
|---|---|---|
| 11 | Cannon Barrage | actor |
| 26 | Dual Implosion | target |
| 95 | Enfeeble | target |
| 214 | Lightning Bolt | actor |
| 235 | Blood Fury | target |
| 242 | Taunt | actor e target |
| 243 | Roar | target |
| 265 | Sachem's Witchcraft | actor |

**FATO:** essas associações existem nos dados. **DESCONHECIDO:** a presença efetiva de cada dummy e a execução de cada canal em sessão. O CSV marca `runtime_binding_proven=no`; o status candidato é `requires_live_dummy_and_opt_in`, não aprovação visual nem cobertura runtime. Não generalizar 8 candidatos para 8 skills integralmente corrigidas.

Os 955 sistemas identificados em F eram condições do PAR; o cruzamento com filhos billboard, posições não zero e canais com dummy configurado reduz o conjunto relevante. Os demais sistemas não devem receber essa regra por nome ou similaridade visual.

## Testes executados

- Fixture de arquivos PAR v14 e EFF v6: parent offset Z=1, dummy Z=30 e filho Z=2. Padrão desligado produz centro GL Y=33; ligado com vínculo produz Y=31, conforme a substituição DX9. Cobre MODEL e STRIP e canais actor/target/item.
- `_bModelDir=true` e dummy ausente preservam o caminho anterior mesmo com switch ligado. A fixture da nova auditoria identifica os mesmos quatro candidatos sintéticos e exclui o caso de direção habilitada.
- Configuração Debug (`CMAKE_BUILD_TYPE=Debug`, `TOP_BUILD_UI_TESTS=ON`) e build completo incluindo `top_gl`: exit 0. CTest completo: **143/143**. Logs: `build/sk218g-final-build.log`, `build/sk218g-final-ctest.log`.
- Fixture focal ASan/UBSan: **1/1**, com `ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1`. Log: `build/sk218g-asan-ctest.log`.
- Reexecutei FX/PAR sobre o código e Client atuais em `build/fx-audit-sk218g` e `build/par-audit-sk218g`: FX conta 1.031 PAR/1.227 EFF/3.476 sistemas/2.832 partes, 27 gaps/307 IDs estáticos, drift 0/falhas estruturais 0; PAR conta 1.031 decodificados/0 falhas e 110 referências ausentes. Os 31 EFF strict-only continuam individualizados e recuperáveis em diagnóstico, 30 com PAR direto e `fboomerff.eff` sem consumidor direto conhecido; nenhum uso runtime provado. Nenhum asset foi removido.
- Sem sessão jogável ou comparação visual DX9/OpenGL. O novo comportamento continua opt-in.

## Ligar, comparar, desligar e reverter

```bash
cmake -S sources -B build -DCMAKE_BUILD_TYPE=Debug -DTOP_BUILD_UI_TESTS=ON
cmake --build build -j4
ctest --test-dir build --output-on-failure

TOP_HISTORICAL_BOUND_BILLBOARD_CENTER=1 \
TOP_HISTORICAL_DUMMY_BIND=1 \
TOP_HISTORICAL_TARGET_DUMMY_BIND=1 \
TOP_HISTORICAL_ITEM_DUMMY_BIND=1 \
TOP_HISTORICAL_BLEND=1 \
TOP_HISTORICAL_MODEL_TEXTURE=1 \
TOP_FROZEN_RESULT_GROUND=1 ./build/top_gl --profile
```

Para a referência GL anterior, manter as variáveis de vínculo e mudar somente `TOP_HISTORICAL_BOUND_BILLBOARD_CENTER=0`. Comparar com DX9 em mesma câmera, pose, instante, actor e alvo: centro do ícone, deslocamento vertical durante a animação e acompanhamento do dummy. Taunt/Roar são exemplos; o mecanismo é compartilhado por todos os candidatos com vínculo disponível.

`SK218G.patch` é incremental sobre E corrigida + F, com caminhos `sources/`. Em uma cópia dessa base: `git apply --check SK218G.patch`, `git apply SK218G.patch`. Para reverter G: `git apply -R SK218G.patch`. O workspace já contém G: não reaplicar aqui.

## Limites e próximos mecanismos comuns

O bridge atual fornece actor/item de jogador e dummies de alvos conhecidos. Actor de monstro e dummies de barco não têm o mesmo caminho completo: o catálogo estático não comprova que esses candidatos tenham vínculo GL. A regra nova não inventa dummies ausentes. A matriz completa de filhos não-billboard, o vínculo de outros donos, o relógio de dano de Part_trace e as resoluções de nomes dos 13 canais do audit continuam preocupações separadas. Cada uma deve ser derivada da cadeia DX9 completa, com correções no motor compartilhado e medições de todos os consumidores.
