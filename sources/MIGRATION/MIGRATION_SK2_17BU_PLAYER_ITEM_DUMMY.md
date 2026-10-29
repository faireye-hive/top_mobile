# SK2.17-BU — ancoragem de efeitos de item no dummy da arma

## Evidência: DX9 original × OpenGL atual × mudança

| Aspecto | DX9 original | OpenGL antes | Mudança desta etapa |
|---|---|---|---|
| Seleção do item | `CCharacter::GetAttackItem()` tenta mão direita e depois esquerda (`arquivo/fontes/Client/src/Character.h:668-672`). | `ActorSkin::playerItemDummyWorldMatrices()` também prioriza slot 9 e depois 6 (`sources/Engine/src/top/ActorSkin.cpp:1438-1462`). | Reutiliza a matriz do item já amostrada para o frame atual. |
| Dummy do efeito | `STAttack::FrameMove` chama `ItemEffect` com o dummy configurado pela skill (`arquivo/fontes/Client/src/STAttack.cpp:262-263`). `CCharacter::ItemEffect` obtém a matriz do dummy e associa o efeito ao item/dummy (`arquivo/fontes/Client/src/Character.cpp:836-860`). | O parser preserva o dummy das colunas de item (`sources/Engine/include/top/SkillVisual.h:126-134`), mas o canal de item não o guardava como âncora; as matrizes eram usadas apenas pelos direct strips (`sources/Engine/src/top/EffectSystem.cpp:1718-1725`). | O canal de item mantém o dummy e, quando habilitado, usa a translação da matriz correspondente em `positionForAge` (`sources/Engine/src/top/EffectSystem.cpp:1046-1051,1488-1498`). |
| Ponte de pose | O DX9 consulta o dummy runtime do modelo equipado durante o efeito (`Character.cpp:849-860`). | A ponte envia as matrizes dos dummies do item amostradas do pose atual (`sources/Platform/linux/main.cpp:2842-2845`; cálculo em `ActorSkin.cpp:1432-1460`). | Converte a translação DX XYZ/Z-up para o mundo GL X/altura/Y usando `{m[12], m[14], m[13]}`, consistente com a conversão de âncora de ator existente (`EffectSystem.cpp:1473-1481`). |

## Mudança e ativação

Adicionado `TOP_HISTORICAL_ITEM_DUMMY_BIND=1`, desligado por padrão. O recurso só ancora canais Item/ItemExtra de casts explicitamente marcados como jogador; não afeta alvos, chão, voo nem eventos sem `sourceIsPlayer`. Se a matriz do dummy não estiver disponível, mantém a posição GL anterior. Para desativar, omita a variável ou defina `0`. Após aplicar o patch, reverta esta etapa com `git apply -R SK217BU.patch`.

## Fato, inferência e desconhecido

- **FATO:** o código DX9 vincula o efeito ao item de ataque e ao dummy da skill; a seleção prefere a mão direita e usa a esquerda como fallback.
- **FATO:** o GL já calcula matrizes de dummies dos modelos equipados e as atualiza para o frame atual; antes desta etapa, os efeitos `.eff/.par` de item não usavam esse dummy.
- **INFERÊNCIA:** a âncora de posição agora acompanha melhor a regra do DX9 quando a opção é ligada. A fixture comprova somente a seleção e a conversão de posição em dados sintéticos.
- **DESCONHECIDO:** não houve comparação visual com o cliente DX9. Esta alteração transfere apenas posição; orientação completa do item, seleção por equipamento em situações fora do jogador e canais de dummy do alvo continuam pendentes.

## Verificação executada

- Configuração Debug e build completa, incluindo `top_gl`: concluídas com exit 0.
- CTest completo: 141/141 aprovados. CTest focal (`ui05g_historical_effect_resources` e `ui05g_skill_channel_composition`): 2/2 aprovados.
- Build focal com ASan/UBSan passou. O executável `--fixture-only`, iniciado da pasta `build/`, passou com `ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1`.
- O CTest `ui05g_skill_channel_composition` passou também no sub-build sanitizado. O CTest de smoke real `ui05g_historical_effect_resources` falhou nesse sub-build porque procurou `../Client` relativo a `build/sk217bu-asan` (`build/Client` não existe); a execução Debug normal da mesma suíte, a partir de `build/`, passou. Essa falha é de caminho de assets, não evidência de falha do sanitizer.
- A fixture compara a posição anterior com a posição do dummy de item usando `TOP_HISTORICAL_ITEM_DUMMY_BIND=0/1`; não usa assets do Client nem afirma paridade visual.
