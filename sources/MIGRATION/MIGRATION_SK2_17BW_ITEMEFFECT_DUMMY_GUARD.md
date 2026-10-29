# SK2.17-BW — regra de dummy obrigatório para ItemEffect

## Resumo

O DX9 não cria efeitos de canal Item/ItemExtra quando o dummy de item configurado é exatamente `-1`. O GL agora reproduz esse filtro quando `TOP_HISTORICAL_ITEM_EFFECT_REQUIRE_DUMMY=1`; a opção fica desligada por padrão. A auditoria FX lista o dummy de cada canal e verifica os callsites e guards DX9/GL diretamente no código.

## Comparação

| Original DX9 | Porte GL antes desta etapa | Mudança |
|---|---|---|
| `arquivo/fontes/Client/src/STAttack.cpp:262-263` chama `ItemEffect` para os canais item1/item2 e passa `sItemDummyLink`. | `sources/Engine/include/top/SkillVisual.h:126-134` lia `itemDummy`, mas `EffectSystem.cpp:864-885` transformava os canais em `RequestedId` sem aplicar a regra de `ItemEffect`. | `EffectSystem.cpp:83-86,936-945` adiciona `TOP_HISTORICAL_ITEM_EFFECT_REQUIRE_DUMMY` e ignora apenas `requested.item && requested.dummy == -1`. Actor e Target não são filtrados por esta regra. |
| `arquivo/fontes/Client/src/Character.cpp:836-864`: `ItemEffect` retorna `false` se `nEffectID==0` ou `nItemDummy==-1`; dummy válido tenta matriz do item e, se ela falha, ainda cria efeito seguindo o item. | Com dummy `-1`, o GL podia agendar/desenhar o canal no ponto do evento, pois nenhuma rejeição histórica era feita. | A fixture comprova que o comportamento desligado continua como antes; ligado, os dois canais sintéticos com dummy `-1` são suprimidos; um canal com dummy `0` continua elegível. |

## Dados e auditoria

O Client atual contém **50 canais Item/ItemExtra em 46 skills**. Três canais usam dummy `-1`: skill IDs `0086` (efeito 218), `0087` (218) e `0098` (159). O auditor chega a esses valores lendo as mesmas colunas que `SkillVisual.h` (`53` para dummy, `54/55` para IDs de item) e agora os grava em `fx_skill_item_channels.csv`.

O audit também procura `CCharacter::ItemEffect` e seu teste `nItemDummy == -1`, os dois callsites de `STAttack` que passam `sItemDummyLink`, e o gate/filtro dentro de `EffectSystem::emit`. A execução no Client terminou com `item_effect_dummy_dx9_rule_and_gl_gate_verified=true`, 50 canais, 3 canais/3 skills com `-1`; as linhas DX9 registradas foram 262 e 263. `runtime_use_proven` continua `no`: os dados são configuração estática, não prova de execução.

## Fatos, inferências e desconhecidos

**FATOS:** a condição exata no DX9 é igualdade com `-1`, antes da busca do item. O despacho de ataque fornece o dummy configurado. Os dados têm os três canais acima. O GL usa o filtro apenas no switch opt-in.

**INFERÊNCIA:** para uma habilidade cujo item dummy é `-1`, o GL ligado agora deixa de produzir uma instância que o `ItemEffect` DX9 recusaria. Isso alinha o dispatch estático; não demonstra que as habilidades sejam executadas nas mesmas condições em jogo.

**DESCONHECIDO:** frequência real de execução dos três registros e aparência visual quando o switch está ligado. Não houve comparação visual com o cliente DX9.

## Testes, uso e reversão

- Fixture de efeito: opção desligada mantém dois canais do caso sintético; opção ligada suprime ambos com dummy `-1`; dummy não negativo permanece elegível.
- Fixture do auditor verifica detecção de regra DX9, parâmetro passado pelo callsite e gate/filtro GL; relatório CSV verifica a coluna do dummy.
- Ativar: `TOP_HISTORICAL_ITEM_EFFECT_REQUIRE_DUMMY=1`.
- Desativar: remover a variável ou defini-la como `0`.
- Reverter em uma cópia onde o patch foi aplicado: `git apply -R SK217BW.patch`.

Build Debug incluindo `top_gl` terminou com exit 0; CTest terminou com 141/141. Fixtures `top_ui05g_tests --fixture-only` e `top_fx_coverage_tests` passaram sob ASan/UBSan (`ASAN_OPTIONS=detect_leaks=0`, `UBSAN_OPTIONS=halt_on_error=1`). O teste sanitizado do auditor precisou ser iniciado da raiz do repositório para resolver os caminhos relativos de `sources/`; a execução dessa forma terminou com exit 0. Nenhuma comparação visual foi feita.
