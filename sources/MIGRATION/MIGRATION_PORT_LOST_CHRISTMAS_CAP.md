# PORT — Lost Christmas Cap (3888)

## Regra histórica

`ItemEffect.lua::ItemUse_DSSDM` recusa uso enquanto navega; em terra aplica `STATE_PKSFYS`, nível 1, por 300 segundos. `skilleffect.lua::State_PKSFYS_Add` dá +30 DEF no nível 1 (nível 10 dá +150).

## Implementação OpenGL

O callback atualiza o temporizador de defesa de consumíveis já usado por Harden Potion/Festive Wine e inclui +30 DEF no cálculo defensivo. Um efeito de nível 10 permanece ativo diante do nível 1; item e save só são alterados se o fluxo puder ser concluído.

## Limite conhecido

O estado é runtime-only e não sobrevive ao reinício do cliente. A skill Harden não foi alterada.
