# SK2.17-V — auditor de proprietário dos CMPStrip diretos

## Objetivo e dado medido

O ranking atual do FX audit marca CMPStrip direto como parcial: seis trailers em seis arquivos .par, associados a 16 IDs de skills no grafo de dependências. Esse ranking importava um CSV histórico separado e não verificava, por trailer, qual canal o referencia nem qual proprietário o DX9/GL consegue fornecer.

O novo top_direct_strip_anchor_audit decodifica o trailer CMPStrip diretamente de cada .par em Client/effect, cruza referências de dependencies.csv e registra os pontos de ligação no DX9 e os provedores de matrizes do porte. Não reutiliza a tabela manual DIRECT_CONTROLLER_STRIPS_SK212.csv como fonte primária.

## Original DX9 x porte GL x resultado

| Original DX9 | Porte GL atual | Resultado da auditoria |
|---|---|---|
| CMPStrip::Play() lê dois dummies de _pItem, senão de _pCha; sem nenhum deles retorna antes de iniciar (arquivo/fontes/Engine/src/MPModelEff.cpp:1251-1272). GetTrack() captura as translações dos dois dummies (MPModelEff.cpp:814-860). | A coleta ocorre só em Cast qualificado como ator/item do jogador (sources/Engine/src/top/EffectSystem.cpp:1389-1396); os provedores atuais são os dummies do corpo e da arma equipada (sources/Platform/linux/main.cpp:2841-2843; ActorSkin.cpp:1408-1468). | A tabela do audit traz por arquivo os IDs dos dummies, canais referenciados, linhas dos dois lados e avaliação de evidência. |
| CMPPartCtrl define ligação para personagem e item (arquivo/fontes/Engine/src/MPParticleCtrl.cpp:326-340). `CEffectObj::SetEffect` chama ambos os setters conforme o tipo de owner (`arquivo/fontes/Client/src/EffectObj.cpp:1048-1060`); MPEffectCtrl também chama `SetStripItem` condicionalmente (MPEffectCtrl.cpp:309-315,415-420). | O GL tem matriz para o corpo do jogador e dummies do item equipado, não um provedor genérico de NPC/monstro/item de cena (ActorSkin.cpp:1408-1468). | A primeira execução do audit V omitiu `Client/src/EffectObj.cpp` e por isso marcou incorretamente o setter actor como ausente; SK2.17-BB corrige a medição. Nenhum desses callsites estáticos prova uso runtime de cada arquivo/skill. |

## Resultado Client e classificação

Comando executado: 1.031 .par de Client/effect, seis trailers em seis arquivos, zero falhas de parsing/decodificação. Há referências a 16 IDs únicos e três canais: actor, item e impact_ground. Um trailer (01030010.par) não tem linha de dependência no CSV atual. O detalhamento está em build/sk217v-direct-strip-audit/direct_strip_anchor_coverage.csv.

- swordlight02.par traz canais item; seus dummies podem ser atendidos pelo item equipado quando o modelo e os dummies correspondem.
- xzsb1.par, xzsb2.par e mosterlight.par aparecem como canais actor; o GL tem matrizes do jogador, mas não matrizes genéricas de outros personagens. O callsite DX9 de personagem em `EffectObj.cpp:1055` foi omitido na primeira execução e confirmado na revisão BB; associação/runtime continuam desconhecidos.
- 01000012.par aparece em canais item e impact_ground. O caminho de ground não fornece, por si só, um proprietário com os dummies 1 e 2.
- 01030010.par tem trailer válido sem dependência direta no grafo de skills examinado.

**FATO:** trailer presente e ligação em tabela não provam que CMPStrip::Play() seja chamado com um proprietário válido. **INFERÊNCIA:** os canais actor/item indicam candidatos a origem, mas não comprovam o objeto concreto que fornece os dummies em runtime. **DESCONHECIDO:** uso por caminhos dinâmicos fora dos arquivos DX9 examinados e quais das 16 skills realmente reproduzem cada trailer no cliente.

Não alterei o renderer: faltam matrizes runtime dos proprietários não jogadores e uma ligação comprovada entre as referências de canal e a chamada DX9 de Play(). Inventar um ponto ou reutilizar dummies do jogador mudaria a semântica. O suporte atual permanece limitado ao corpo e item do jogador; CMPStrip direto ainda não está validado visualmente.

## Execução

~~~bash
cmake -S sources -B build -DCMAKE_BUILD_TYPE=Debug -DTOP_BUILD_UI_TESTS=ON
cmake --build build --target top_direct_strip_anchor_audit top_direct_strip_anchor_audit_tests -j$(nproc)
ctest --test-dir build -R ui05sk217v_direct_strip_anchor_audit --output-on-failure
build/top_direct_strip_anchor_audit \
  --client Client \
  --out build/sk217v-direct-strip-audit \
  --deps sources/auditoria-historica/dependencies.csv \
  --dx9-model-eff arquivo/fontes/Engine/src/MPModelEff.cpp \
  --dx9-particle-ctrl arquivo/fontes/Engine/src/MPParticleCtrl.cpp \
  --dx9-effect-ctrl arquivo/fontes/Engine/src/MPEffectCtrl.cpp \
  --gl-effect-system sources/Engine/src/top/EffectSystem.cpp \
  --gl-actor-skin sources/Engine/src/top/ActorSkin.cpp
~~~

Build Debug completo concluiu, incluindo top_gl; CTest passou 123/123. O fixture passou também com ASan/UBSan (detect_leaks=0; LSan não executado). Audit Client retornou par=1031, records=6, files=6, decode_failures=0, no_dependency_row=1. A conclusão de ausência de call site a `SetStripCharacter` foi posteriormente corrigida por SK2.17-BB: a execução V omitiu `Client/src/EffectObj.cpp`, que chama o setter em `EffectObj.cpp:1055`. Nenhuma execução visual foi feita.

## Correção de evidência — SK2.17-BB

O chamador DX9 de `SetStripCharacter` é `CEffectObj` (`arquivo/fontes/Client/src/EffectObj.cpp:1048-1056`), fora dos três arquivos inicialmente varridos. O audit estendido confirmou essa ligação e também a ligação de item em `EffectObj.cpp:1057-1060`. Assim, a classificação antiga `actor_setter_callsite_missing` foi substituída por `actor_setter_callsite_present_runtime_unproven`; não há prova de quais recursos executam o callsite nem matrizes GL genéricas de NPC/monstro. Ver relatório `MIGRATION_SK2_17BB_DIRECT_STRIP_CALLSITE_AUDIT_FIX.md` e saída `build/sk217bb-direct-strip-audit/`.
