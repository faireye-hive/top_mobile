# SK2.17-AA — classificação de contagens SNOW/FIRE no audit

## Resumo

O audit Client encontrou 139 sistemas SNOW/FIRE com mais de 24 slots, em 74 `.par` e 44 skills alcançadas. Esse item aparecia como lacuna embora o porte já ofereça o uso da contagem serializada sob `TOP_HISTORICAL_PARTICLE_COUNT=1`. Esta etapa corrige apenas a classificação/nota do audit; o limite conservador e o comportamento do jogo não mudam.

## Evidência: DX9 x porte x mudança

| Aspecto | DX9 original | Porte OpenGL | Mudança do audit |
|---|---|---|---|
| Quantidade | `SetSysNum(iNumPart)` grava `_iParNum` e dimensiona o pool (`arquivo/fontes/Engine/src/MPParticleSys.cpp:269-270`); `_FrameMoveSnow` e `_FrameMoveFire` percorrem `n < _iParNum` (`arquivo/fontes/Engine/src/MPParticleSys.cpp:3524-3582,3734-3757`). | O modo opt-in escolhe `authoredCount` para tipos 1 e 2 (`sources/Engine/include/top/HistoricalParticleCount.h:8-19`); `EffectSystem` consulta o switch no caminho de render (`sources/Engine/src/top/EffectSystem.cpp:1885-1888`) e no ciclo de emissão (`sources/Engine/src/top/EffectSystem.cpp:1117-1122`). | O audit continua medindo 139 sistemas, mas não os classifica como falta de implementação; a nota declara o switch e a limitação de validação visual. |

Traduzir a contagem serializada não altera a API gráfica: o GL simplesmente emite/itera o número de quads correspondente nos seus buffers. O limite de fallback permanece 24 quando a opção está desligada; não é removido nem elevado por padrão.

## FATO, inferência e desconhecido

**FATO:** o código DX9 percorre `_iParNum`; o código GL já pode selecionar a contagem do arquivo para SNOW/FIRE. O audit atual anterior a AA registrou 139 ocorrências, 74 arquivos e 44 skills.

Execução Client atualizada após AA: 1.031 `.par`, 1.227 `.eff`, 3.476 sistemas, 2.832 subefeitos; zero falhas e deriva. O ranking caiu de 24 para **23** gaps, permanecendo em 307 o número de skills com pelo menos um gap.

**INFERÊNCIA:** esses vínculos medem recursos alcançáveis na tabela de dependências, não execução confirmada no cliente.

**DESCONHECIDO:** não houve comparação visual da densidade com DX9. O switch permanece desligado por padrão; a classificação de cobertura não significa equivalência visual.

## Testes

Foi atualizado o teste `ui05sk214_fx_coverage_audit` para manter a feature no CSV de cobertura e removê-la do ranking de lacunas. Build Debug do auditor e CTest completo registrados no estado do porte.
