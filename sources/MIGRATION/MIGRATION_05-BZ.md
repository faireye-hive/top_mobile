# 05-BZ — Abertura do cofre ligada à ação histórica do NPC

## Alteração

- O serviço de cofre deixou de depender de `npc.name` conter a palavra inglesa `Banker`.
- A leitura passiva dos scripts agora reconhece `TriggerAction(..., OpenBank)` somente dentro da função Lua identificada pelo NPC. Textos, comentários e ações de funções vizinhas não habilitam o serviço.
- A interação abre a UI de inventário/cofre apenas para NPCs cuja função histórica declare `OpenBank`; NPCs com nomes localizados ou diferentes passam a funcionar sem heurística pelo nome.
- A cobrança offline existente continua sendo 200G por abertura, conforme a sequência histórica `HasMoney`, `TakeMoney` e `OpenBank`; o cofre continua persistente por personagem.

## Verificação

- Build de `top_gl`, `top_ui05aj_tests` e `top_ui05ad_tests`: aprovado.
- Testes cobrem ação positiva, texto/comentário falso, e os scripts reais de Monica (`r_talk13`) e Belinda (`r_talk81`).
- CTest direcionado e `top_gl --check`: aprovados.

## Limite desta etapa

O parser extrai a presença da ação de abertura, mas ainda não reproduz um motor geral de diálogos Lua nem suas condições e ramificações arbitrárias. A ação de cofre é implementada localmente e sem servidor.
