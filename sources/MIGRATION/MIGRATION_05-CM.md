# Migração 05-CM — Christmas Dinner

## Regra histórica

`ItemUse_SDDC` recusa o uso enquanto navega e define `ATTR_CEXP` usando a tabela
cumulativa `DEXP`:

- níveis 1–9: vai ao marco `nível + 5`;
- níveis 10–29: vai ao marco `nível + 3`;
- níveis 30–59: vai ao próximo nível;
- níveis 60–75: avança 50% da barra seguinte mais 10 EXP;
- níveis 76–85: avança 30% mais 10 EXP; no nível 79, limita o excesso acima
  do próximo marco a 2%;
- níveis 86+: avança 1% da barra seguinte.

## Porte OpenGL

O handler converte os alvos cumulativos para o EXP local do personagem e usa a
mesma rotina de progressão que trata subida de nível, AP/TP, atributos e HP/SP.
O item é consumido e o personagem salvo após a aplicação.

## Verificação e limites

- Fontes: `ItemEffect.lua::ItemUse_SDDC`, `variable.lua::DEXP` e
  `iteminfo.txt::2898`.
- Estado D1: a compilação e o fluxo no jogo/save-reload ainda precisam de
  validação. Nível 100 preserva o item porque não há marco `DEXP[101]` no
  acervo histórico.
