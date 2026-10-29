# SK2.17-BV — vínculo de efeito recebido ao dummy do alvo

## Resumo

Esta etapa porta a posição de acompanhamento dos efeitos do canal Target de `SkillVisual`. O recurso é opt-in por `TOP_HISTORICAL_TARGET_DUMMY_BIND=1`, desligado por padrão. Ele resolve a posição atual do dummy do ator atingido a cada frame e usa a raiz desse mesmo ator quando o dummy solicitado não existe ou não pode ser lido. Não há validação visual DX9/OpenGL; portanto esta etapa não é declarada visualmente fiel.

## Evidência e comparação

| Original DX9 | Porte GL antes desta etapa | Mudança desta etapa |
|---|---|---|
| `SkillRecord.h:198-202` define `sTargetDummyLink` e separa efeito de alvo, chão e água; `SkillRecord.cpp:422-430` lê esses campos da tabela. | `SkillVisual` já decodificava os canais de alvo, mas `EffectEvent` não transportava a identidade do ator atingido e o `positionForAge` de `EffectSystem.cpp` interpolava a posição do evento/terreno. | `EffectEvent::targetActorIndex` carrega a identidade: `-1` jogador, `>=0` índice de monstro, `-2` desconhecido. `EffectSystem.cpp:859-896,934` conserva o dummy do canal Target. |
| `EffDelay.cpp:35-39` chama `SelfEffect(effectID, targetDummy, ...)` no personagem que recebeu o hit. | `PlayCombat.cpp` emitia `SkillHit` com coordenadas, sem identidade persistente do alvo. | `PlayCombat.cpp:373-379,568-590,1039-1046` marca o índice real do alvo para hits diretos, de área, e hits recebidos pelo jogador. |
| `Character.cpp:940-952` pede a matriz runtime do dummy. Se válida, chama `setFollowObj(..., NODE_CHA, dummy)` e emite em `mat._41`; sem dummy válido, segue `NODE_CHA` e emite em `GetPos()`. | O GL não tinha vínculo do canal Target a dummies/raiz do ator após o evento. | `ActorSkin.cpp:1454-1493` obtém as matrizes do pose atual do monstro e a raiz do mesmo slot. `main.cpp:2845-2857` coleta isso depois de `actors.build`; `EffectSystem.cpp:1524-1538` acompanha dummy válido e cai para a raiz do mesmo ator. |

**Tradução de coordenadas:** as matrizes preservam a convenção D3DX de vetor-linha e posição em `[12..14]`; o GL lê a translação e remapeia DX9 `(X,Y,Z-up)` para GL `(X,height,Y)`, `{m[12],m[14],m[13]}`. É a mesma tradução já usada para dummies do corpo no porte. Esta etapa consome apenas translação; orientação completa do dummy não foi portada.

## Fatos, inferências e desconhecidos

**FATOS:** a estrutura e o parser DX9 identificam um dummy separado para o efeito recebido; `EffDelay` passa esse dummy a `SelfEffect`; `SelfEffect` segue a matriz do dummy se `GetObjDummyRunTimeMatrix` retornar sucesso e, caso contrário, segue a raiz do personagem. No GL, eventos de combate agora identificam o ator-alvo e o bridge coleta as matrizes depois da construção/renderização da pose. Testes sintéticos verificam dummy válido, fallback de raiz e comportamento inalterado com a opção desligada.

**FATO DOS DADOS DO CLIENT:** contei as colunas 62/63 de `Client/scripts/table/skillinfo.txt`, que `SkillVisual.h:138-143` lê como target effect/dummy: 223 registros têm efeito de alvo; 130 têm dummy `>=0` e 93 têm dummy `-1`. Entre as skills de Crusader auditadas, IDs 85/87/88 têm dummy 2; IDs 86/453 usam `-1`. Esses dados demonstram exemplos para ambos os ramos, mas não provam que o evento seja executado em jogo.

**INFERÊNCIA:** amostrar a pose desenhada do monstro deve aproximar o vínculo por frame da matriz runtime do DX9, desde que o mapeamento de índices de monstros do `Play` corresponda ao de `ActorSkin`. O teste sintético e a comparação estática não comprovam essa correspondência visual em uma sessão real.

**DESCONHECIDO:** equivalência da orientação/rotação do objeto de efeito; precisão de offsets/escala entre `GetPos()` DX9 e raiz GL; correspondência visual de animação para poses não visíveis/culladas; e comportamento quando um ator sai da lista durante o tempo de vida do efeito. É necessária comparação visual com o cliente DX9 para declarar fidelidade.

## Alteração e reversão

- Alterações C++ limitadas ao transporte da identidade do alvo, coleta das matrizes/raízes de atores e avaliação da posição no canal Target; fixture cobre os dois caminhos.
- Ativar: `TOP_HISTORICAL_TARGET_DUMMY_BIND=1`.
- Desativar: remover a variável ou definir `TOP_HISTORICAL_TARGET_DUMMY_BIND=0`.
- Reverter o patch incremental: `git apply -R SK217BV.patch` (apenas em uma cópia à qual `SK217BV.patch` tenha sido aplicado).

## Testes e auditoria

Build Debug completo, incluindo `top_gl`, terminou com exit 0; CTest: 141/141 passou. A fixture `top_ui05g_tests --fixture-only` passou com ASan/UBSan (`detect_leaks=0`, `UBSAN_OPTIONS=halt_on_error=1`). A tentativa de compilar `top_actor_skill_pose_tests` no sub-build sanitizado falhou no link por referência ausente `top::Font::glyph`; portanto esse executável não foi testado com sanitizers. A auditoria somente-leitura do Client foi executada com `top_fx_coverage_audit`: 1.031 PAR, 1.227 EFF, 3.476 sistemas, 2.832 partes, 27 lacunas/307 IDs de skill estáticos, deriva 0, falhas de decodificação 0 e strips sem correspondência 0. O ranking geral não mede este vínculo de dummy como feature; esta implementação foi selecionada por comparação estática direta entre o call site DX9 e o canal Target existente, não por posição no ranking.

Não houve comparação visual. A orientação completa permanece fora do escopo deste patch. Um achado independente para etapa futura é `ItemEffect`: DX9 rejeita dummy de item `-1` (`Character.cpp:836-864`), enquanto o GL atual mantém canais Item sem dummy; não foi alterado aqui.
