# 05-BD — ItemStat

Os dez itens históricos `899–903` e `3109–3113` agora leem `Server.Restore.Attr` de `variable.lua`. O uso verifica a faixa histórica 5–130 e a navegação, altera somente o atributo base correspondente, devolve ao jogador a quantidade de pontos indicada por um valor negativo, recalcula os atributos derivados, consome o item e grava o save.

Falhas de limite, tabela ausente, loja aberta ou navegação preservam o item e o estado do personagem.

## `PORT_APPROX` — limitação aberta

O servidor histórico também bloqueava o uso durante um stall privado; esse sistema de stall não existe no cliente offline. O cliente bloqueia uma loja aberta e o barco, mas não há estado de stall para consultar.

Fonte histórica: `Client/scripts/offline/calculate/variable.lua` (`Server.Restore.Attr`) e `Client/scripts/offline/script/calculate/ItemEffect.lua` (`ItemStat`).
