# SK2.17-AO — controlador DUMMY ligado aos dummies do item

## Resultado

O porte tinha type 16 encaminhado para `groundQuad`, embora o DX9 obtenha os dummies 1 e 2 do objeto associado e use esse segmento para emitir/mover as partículas. Foi adicionado um controlador opt-in genérico para canais de item do jogador, usando matrizes de `ActorSkin`. A auditoria AP corrigiu a prova de uso: os dados atuais não ligam nenhum canal Item/ItemExtra a PAR type 16; este controlador permanece experimental e sem consumidor real identificado no Client atual.

Ativação: `TOP_HISTORICAL_DUMMY_CONTROLLER=1`. Desativação: omitir a variável ou defini-la como `0`. Reversão: `git apply -R SK217AO.patch`.

## Fatos: DX9, porte e mudança

| Original DX9 | Porte OpenGL antes da etapa | Mudança |
|---|---|---|
| `CCharacter::GetAttackItem` escolhe mão direita e recorre à esquerda (`arquivo/fontes/Client/src/Character.h:668-673`). `ItemEffect` busca o dummy pedido e segue o item (`arquivo/fontes/Client/src/Character.cpp:836-864`). | `ActorSkin::playerItemDummyWorldMatrices` já calcula matrizes dos dummies do item equipado, prioriza slot 9 e recorre ao slot 6 (`sources/Engine/src/top/ActorSkin.cpp:1432-1462`); `main.cpp` as fornecia à trilha direta (`sources/Platform/linux/main.cpp:2841-2843`). | A mesma amostra de matriz passa a alimentar o type 16 dos canais Item/ItemExtra de conjuração do jogador, sem deslocamentos artificiais (`sources/Engine/src/top/EffectSystem.cpp:1982-1984,2101-2117`). |
| `CMagicEff::setFollowObj` configura `SetItemDummy(item,1,2)` para efeitos sem CMPStrip (`arquivo/fontes/Client/src/EffectObj.cpp:1063-1067`). `GetDummyPosList` lê ambos os dummies, calcula direção/distância e usa o dummy 2 como origem (`arquivo/fontes/Engine/include/MPParticleSys.h:503-520`). | O renderer encaminhava type 16 ao quad preso ao terreno (`sources/Engine/src/top/EffectSystem.cpp:2690-2694`, comportamento mantido como fallback). | A timeline recebe IDs 1/2, simula nascimento/vida/movimento do pool e não desenha partículas se as duas matrizes faltarem (`sources/Engine/include/top/HistoricalDummyController.h:35-85`; `EffectSystem.cpp:2101-2117,2179-2185`). |
| `_CreateDummy` incrementa o relógio compartilhado, normaliza a direção, calcula posição e vida aleatória (`arquivo/fontes/Engine/src/MPParticleSys.cpp:1746-1779`). `_FrameMoveDummy` avança o filho `.eff`, atualiza a curva, velocidade e aceleração com sinal aleatório (`:1782-1846`). | O GL usava a idade analítica do efeito e não tinha estado por slot de DUMMY. | `TOP_HISTORICAL_DUMMY_CONTROLLER=1` ativa um pool por instância/perfil; ele consome o delta da idade da instância ao construir o quadro (`EffectSystem.cpp:2101-2117`). A conversão de matriz para DX9 XYZ e o retorno ao GL X/Y-up/Z ocorrem nos limites citados (`EffectSystem.cpp:2108-2110,2315-2322`). |

## Uso medido e `.eff` recusados pelo parser estrito

O Client lista `sceneffectinfo` ID 152 como `01040009.par` (`Client/scripts/table/sceneffectinfo.txt:76`), mas isso não demonstra ligação com Fireshield. O parser DX9 lê `sItemEffect1/2` nas posições correspondentes às colunas 54/55 zero-based (`arquivo/fontes/Libraries/common/src/SkillRecord.cpp:389-407`); na linha 24 do `skillinfo.txt`, ambas são `0,0`. O valor 152 está na coluna `sWhoped` (`SkillRecord.cpp:420-424`) e `CHitRepresent::Exec` o passa a `PlayEnvSound` (`arquivo/fontes/Client/src/EffDelay.cpp:27`). O canal de item só chama `ItemEffect` quando o ID do canal é positivo (`STAttack.cpp:262-263`).

O novo relatório `fx_skill_item_channels.csv` cruza os campos Item/ItemExtra, IDs de `sceneffectinfo` e tipos dos PARs do Client. Ele encontrou 50 canais não nulos em 43 linhas/skills; nenhum resolve para type 16. As linhas que citam o PAR type16 `01040009.par` não existem nesse CSV. O modelo `01040009.lgo` ter dummies 1/2 e a entrada de catálogo ID 152 são fatos de inventário, mas não provam que o controlador seja chamado. O teste do controlador é sintético, não uma reprodução de Fireshield.

Auditoria reexecutada nesta etapa (`top_fx_coverage_audit --client Client ...`): 1.031 PAR, 1.227 EFF, 3.476 sistemas, 2.832 partes EFF, 26 lacunas, drift zero e zero falhas estruturais no modo diagnóstico. Das 31 falhas do decoder estrito, 31/31 foram decodificadas estruturalmente no modo diagnóstico; 30 têm referência PAR direta e 1 (`fboomerff.eff`) tem associação por `fboomerff.par`, ID 919 e skill 308. Nenhuma foi removida ou classificada como inútil; o audit não comprova execução runtime. Parâmetros `RectPlane`/LGO que falham no parser estrito continuam sem justificativa para serem normalizados ou descartados.

**Limite do grafo do audit:** `fx_skill_gaps.csv` continua baseado no `dependencies.csv` e nas dependências PAR→EFF. O novo `fx_skill_item_channels.csv` fornece uma trilha separada para canais Item/ItemExtra; sua presença prova ligação declarada nos dados, não execução durante uma sessão. Neste Client, não há ligação de skill/item ao type16 demonstrada por essa trilha.

## Aproximações e desconhecidos

- **Fato:** o LGO `01040009.lgo` tem IDs de dummy 1/2. O PAR correspondente é cadastrado no catálogo, mas a auditoria não encontrou canal de skill/item apontando para seu ID.
- **Inferência:** nas chamadas de item do jogador, o snapshot de dummies depois de `ActorSkin::build` representa a pose desenhada naquele quadro. É a estratégia existente de amostragem do porte; não houve confronto visual quadro a quadro com DX9.
- **Aproximado:** o DX9 usa `rand()` global para vida, posição e sinal da aceleração; a timeline usa hash determinístico por instância/slot. Ela consome o relógio dos quadros desenhados e não reconstrói a sequência global aleatória do cliente.
- **Pendente:** a chamada GL atual emite todos os canais de cast quando recebe o evento; esta etapa não adiciona a temporização por keyframe dos canais. O teste verifica apenas a máquina de estados sintética, não um efeito do Client.
- **Pendente:** efeitos de item de outros personagens/NPCs, orientação completa do item, relógio histórico de cada filho `.eff` e prova visual DX9/OpenGL. Este patch só liga canais de item emitidos pelo jogador; não inventa âncoras de outros donos.
- A amostra traduz armazenamento de matriz do GL para coordenadas DX9 XYZ dentro do controlador e reordena XYZ/Z-up para X/Y-up/Z do renderer no limite de desenho. Não muda convenção de blend, winding ou culling.

## Testes e comandos

Executados:

- `cmake -S sources -B build -DCMAKE_BUILD_TYPE=Debug -DTOP_BUILD_UI_TESTS=ON` — sucesso.
- `cmake --build build -j$(nproc)` — sucesso, incluindo `top_gl`.
- `ctest --test-dir build --output-on-failure` — **136/136** aprovados.
- Fixture nova `sk217ao_dummy_controller` — pool, step compartilhado, movimento, matrizes sintéticas e conversão do eixo de altura; aprovada.
- `build/top_fx_coverage_audit --client Client --out build/sk217ao-current-audit --deps sources/auditoria-historica/dependencies.csv --table sources/auditoria-historica/fx_port_coverage.csv --source sources/Engine/src/top/EffectSystem.cpp --controller-strips sources/auditoria-historica/DIRECT_CONTROLLER_STRIPS_SK212.csv` — terminou com `drift=0`, `failures=0`.

Teste visual (ainda necessário; comparar a mesma skill e item equipado com DX9):

```bash
TOP_HISTORICAL_DUMMY_CONTROLLER=0 ./build/top_gl --profile
TOP_HISTORICAL_DUMMY_CONTROLLER=1 ./build/top_gl --profile
```

Não usar Fireshield como caso de validação desse controlador: os campos item da skill estão zerados. Primeiro identifique um consumidor real no CSV atualizado; até lá, os comandos só exercitam a rota opt-in com fixtures/dados que efetivamente a acionem. Não marcar como fiel/validado sem comparação visual DX9/OpenGL.
