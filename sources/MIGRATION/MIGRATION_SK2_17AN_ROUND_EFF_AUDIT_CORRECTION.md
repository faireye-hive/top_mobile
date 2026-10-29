# SK2.17-AN — correção da evidência do filho EFF em ROUND

## Resultado

Corrigidos dois textos na tabela de cobertura que diziam que o ROUND OpenGL não tinha timeline para o filho `.eff` ou que esse relógio continuava pendente. A SK2.17-AM já implementou essa timeline opt-in; os relatórios incorretos podiam levar uma leitura futura a tratar o recurso como ausente. Esta etapa altera somente a documentação de cobertura e sua fixture automatizada; não altera o renderer nem o comportamento em runtime.

## Original DX9 x porte GL x mudança

| Original DX9 | Porte GL atual antes desta etapa | Mudança |
|---|---|---|
| `_FrameMoveRound` chama `CMPModelEff::FrameMove(dt)` uma vez antes de atualizar as partículas (`arquivo/fontes/Engine/src/MPParticleSys.cpp:2926-2929`). O relógio do filho descarta excesso e avança no máximo um quadro por chamada (`arquivo/fontes/Engine/src/MPModelEff.cpp:500-540`). | A SK2.17-AM mantém `roundEffTimelines` por perfil e avança a timeline ao receber `EffectSystem::step(dt)` (`sources/Engine/include/top/EffectSystem.h:224`; `sources/Engine/src/top/EffectSystem.cpp:1105-1108,1200-1202,2374-2393`). | A linha `round_nested_eff_frame_step` da tabela ainda afirmava ausência da timeline. O texto agora descreve o caminho opt-in já existente, sem alterar seu estado desligado por padrão (`sources/auditoria-historica/fx_port_coverage.csv:41`). |
| O tipo histórico ROUND é 9 (`arquivo/fontes/Engine/include/MPParticleSys.h:194`). | O audit identifica a feature por type 9 e referência `.eff` (`sources/Engine/include/top/FxCoverageAudit.h:188-191`). | A fixture garante que token e nota da regra continuem associados à implementação e rejeita a afirmação antiga (`sources/tests/FxCoverageAuditTests.cpp:101-110`). |

## Fato, inferência e desconhecido

- **Fato:** a timeline do filho existe no porte com o switch `TOP_HISTORICAL_ROUND_TIMELINE=1`, dependente de `TOP_HISTORICAL_PARTICLE_EXTENDED=1`; os caminhos e linhas estão citados acima.
- **Fato:** a auditoria Client atual encontrou 10 pais ROUND com filho `.eff`, em 8 `.par` e ligados estaticamente a 4 skills; a feature ROUND em geral alcança 7 skills.
- **Inferência:** corrigir a nota impede que relatórios futuros recomendem novamente a implementação já feita.
- **Desconhecido:** as referências estáticas não provam execução em runtime. A sequência global de `rand()`, a cadência efetiva do cliente DX9 e a paridade visual ainda precisam de evidência/comparação.
- Não houve mudança de renderer; ROUND continua aproximado e sem validação visual.

## Verificação

Comando de audit executado: `build/top_fx_coverage_audit --client Client --out build/sk217an-audit --deps sources/auditoria-historica/dependencies.csv --table sources/auditoria-historica/fx_port_coverage.csv --source sources/Engine/src/top/EffectSystem.cpp --controller-strips sources/auditoria-historica/DIRECT_CONTROLLER_STRIPS_SK212.csv`.

Resultado: 1.031 `.par`, 1.227 `.eff`, 3.476 sistemas, 2.832 partes EFF, 26 lacunas, 307 skills com ao menos uma lacuna, drift zero e falhas estruturais zero. O relatório traz 31 recusas estritas de `.eff`, mas todos os 31 foram decodificados em modo diagnóstico; 30 têm referência direta em `.par` e o restante tem `.par` homônimo ligado à cena 919/skill 308. Execução em runtime não comprovada.

Build Debug completo, incluindo `top_gl`, e CTest **135/135** passaram após a correção. A fixture agora verifica que o audit não volte a declarar ausente a timeline que já existe. Nenhuma comparação visual DX9/OpenGL foi feita.

## Reversão

Reverter somente este patch: `git apply -R SK217AN.patch`. A regra de cobertura volta ao texto incorreto se a reversão for aplicada; nenhum comportamento do jogo muda.
