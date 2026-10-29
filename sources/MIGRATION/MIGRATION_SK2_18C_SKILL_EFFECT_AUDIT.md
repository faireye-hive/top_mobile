# SK2.18-C — auditoria de quatro efeitos de skill e dos EFF strict-only

## Escopo e resultado

Esta etapa atualiza a auditoria estática do Client e cruza os IDs 93 (Frozen Arrow), 81/3 (Illusion Slash), 242 (Taunt) e 84 (Berserk) com as dependências históricas e os caminhos de execução do porte. Não altera comportamento de jogo.

Auditoria executada da raiz:

```bash
build/top_fx_coverage_audit --client Client --out build/fx-audit-skill-review \
  --deps sources/auditoria-historica/dependencies.csv \
  --table sources/auditoria-historica/fx_port_coverage.csv \
  --source sources/Engine/src/top/EffectSystem.cpp \
  --dx9-source arquivo/fontes/Engine/src/MPResManger.cpp \
  --dx9-particle-source arquivo/fontes/Engine/src/MPParticleSys.cpp \
  --dx9-particle-header arquivo/fontes/Engine/include/MPParticleSys.h \
  --dx9-character arquivo/fontes/Client/src/Character.cpp \
  --dx9-skill-attack arquivo/fontes/Client/src/STAttack.cpp
```

Saída: exit code 0; 1.031 PAR, 1.227 EFF, 3.476 sistemas, 2.832 partes EFF, 27 gaps estáticos, 307 IDs de skill com gaps, deriva 0, falhas estruturais 0. `fx_decode_failures.csv` não tem linhas de falha. Isso não é prova de execução ou paridade visual.

## Original DX9 × porte GL × evidência medida

| Skill / Client | DX9 original | Porte atual / auditoria | Conclusão permitida |
|---|---|---|---|
| Frozen Arrow 93, Magic 20 | `magicsingleinfo.txt:12` declara `icearrowf.eff,icearrowf(a).eff`, velocidade 10, modo 2 e resultado `icearrowh`. `Character.cpp:960-1021` passa `nTargetChaID`; `EffectObj.cpp:235-314` atualiza o alvo móvel, testa proximidade e chama `HitTestMap`. | `PlayCombat.cpp:842-845,1131-1133` cria voos sem `targetActorIndex`. `EffectSystem.cpp:1518-1565` avalia o voo pela trajetória inicial; o vínculo a ator existente só se aplica a `SkillHit` (`:1547-1561`). Auditoria: `feature:snow_fire_step`, `type:2`, `type:6`. | **FATO:** `Part_trace` no GL não recebe o alvo móvel e não reproduz o teste de colisão do controlador nesse caminho. **DESCONHECIDO:** timing/altura visual no Client real; ainda não se alterou movimento. |
| Illusion Slash 81, Magic 18 | `magicsingleinfo.txt:10` declara `04000009.eff`, velocidade 15, modo 3. `EffectObj.cpp:356-399` renderiza antes de mover, testa proximidade/terreno e só então avança o contador de cópias. | `EffectSystem.cpp:1150-1155,1518-1565` usa duração e posição analíticas; SK2.18-B adicionou o contador opt-in. Dependências: voo 195, ator 186, item 218 e impacto 195 (`dependencies.csv`, evidências Client `sceneffectinfo.txt`). Auditoria lista `direct_cmpstrip`, `type:12`, `type:6`. | O contador de cópias cobre apenas parte da rotina. Gaps da auditoria não demonstram, sozinhos, defeito visual. Escala provisória de voo/impacto continua sem validação visual DX9/GL. |
| Illusion Slash 3, Magic 10 | `magicsingleinfo.txt:2` vincula `runningattack.eff`, modo 2; canais Client associam `runatktarget.par` e `yfire.par` (`dependencies.csv`). | Auditoria: `feature:model_delayed_frame_step`, `feature:model_frame_step`, `feature:model_midpoint_position`, `feature:model_nested_eff_frame_step`, `type:1`, `type:5`. | É um registro distinto do ID 81, apesar do mesmo nome traduzido. Não generalizar SK2.18-B para esta variante. |
| Taunt 242 | `dependencies.csv` associa ator 336 (`cfsf.par`) e alvo 337 (`cfsj.par`); Client `skillinfo.txt` declara Taunt na linha 188. | Auditoria: `feature:blast_group_cycle`, `type:3`, `type:6`. O bridge de alvo recebe índice para efeitos `SkillHit` quando informado, `EffectSystem.cpp:1089-1093,1547-1561`. | Há lacunas medidas nos sistemas de partículas alcançados; não é evidência suficiente para decidir se o erro percebido é ancoragem no ator/alvo, geometria ou ciclo do controlador. |
| Berserk 84 | `dependencies.csv` associa ator 117 (`spire01.par`) e ground 236 (`firegush.par`); Client `skillinfo.txt:83`. | Auditoria: `feature:ripple_emission_recycle`, `feature:snow_fire_step`, `type:1`, `type:4`, `type:6`. O término de ação já agenda o canal de chão em `PlayCombat.cpp:215` e os estados do buff têm caminho próprio. | Visual de cast, impacto no chão e duração/estado do buff são rotas distintas. Os gaps não isolam qual delas causa a diferença vista pelo usuário. |

## Voos `Part_trace`: fato, tradução e trabalho pendente

- **FATO DX9:** `PointInstrPointRange` compara apenas X/Y do plano (`arquivo/fontes/Engine/include/MPParticleSys.h:48-53`; OpenGL X/Z). `Part_trace` chama `Render` antes da simulação, limita o passo a 1,5, para ao entrar na faixa de 1 unidade ou abaixo do terreno (`EffectObj.cpp:243-271`), e ajusta direção/posição quando o alvo muda (`:274-310`). `SkyEffect` passa o ID do ator para `Emission` (`Character.cpp:1018-1021`).
- **FATO GL:** a trajetória é derivada da posição inicial e da idade (`EffectSystem.cpp:1518-1565`). Eventos de voo no combate não carregam ID de alvo (`PlayCombat.cpp:842-845,1131-1133`). A ponte de raízes/dummies é preenchida no `main.cpp:2845-2857`, mas somente com `TOP_HISTORICAL_TARGET_DUMMY_BIND=1`; o cálculo corrente a usa para impacto, não para voo (`EffectSystem.cpp:1547-1561`).
- **TRADUÇÃO:** o teste de faixa DX9 no plano X/Y corresponde a X/Z no mundo OpenGL X/altura/Z. A colisão do projétil DX9 compara altura Z com `GetGridHeight`; no porte, a consulta equivalente é `MapFile::heightAt(x,z)`. Nenhuma mudança desta etapa altera coordenadas ou colisão.
- **INFERÊNCIA:** o voo do Frozen Arrow pode parecer quase certo em alvo parado e divergir quando ele se move. A relação é coerente com o modo 2 e a ausência do índice no GL, mas precisa de captura comparável para confirmar a manifestação visual.
- **DESCONHECIDO:** qual callsite DX9 específico fornece cada lançamento de skill 93/81 em runtime, comportamento com alvo removido e comparação quadro a quadro no Client real.

## `.eff` strict-only: utilidade atual

O CSV atual (`build/fx-audit-skill-review/fx_eff_usefulness.csv`) contém 31 arquivos com falha no decoder estrito; todos foram recuperados pelo parser diagnóstico como `decoded_with_raw_model_params`, sem parâmetros animados não finitos. Trinta têm referência estática direta em PAR. O CSV registra evidência de enumeração/carga/indexação pelo catálogo DX9 para os 31, mas não comprova que cada um foi instanciado durante uma sessão.

Os 31 nomes, consumidores, IDs `sceneffectinfo`, modelos e contagens estão preservados individualmente no CSV. `fboomerff.eff` é a exceção sem referência direta encontrada: sua associação com `fboomerff.par`/ID 919 é apenas basename; a inspeção desse PAR não encontrou subefeito `.eff`. Não classificar como usado nem como inútil: consumidor runtime continua desconhecido. Os demais 30 estão referenciados por PAR e devem manter suporte; a necessidade de remover validação estrita de parâmetros-base só se justifica após fixture e equivalência de interpretação, não por supor que o arquivo não seja usado.

## Testes e limites

- Auditoria acima: executada, exit code 0.
- Não foi executada compilação ou CTest nesta etapa, pois não houve mudança de código.
- Não houve execução visual do Client nem comparação DX9/OpenGL.
- Nenhum comportamento foi marcado como fiel/validado. Sem alteração no funcionamento de Frozen Arrow, Illusion Slash, Taunt ou Berserk.

## Próxima etapa

Implementar e testar, isoladamente, o caminho dinâmico de `Part_trace` para voos cujo evento consiga identificar o ator-alvo, incluindo a regra de término por proximidade/terreno e a emissão do `Result` no ponto efetivo de parada. Primeiro cobrir Frozen Arrow 93; ampliar a outros IDs apenas quando o audit provar o vínculo.
