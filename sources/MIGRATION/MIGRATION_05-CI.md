# Migração 05-CI — Energy Potion

## Regra histórica

- Item 1857 chama `ItemUse_PKJSYS` com state level 10 por 180 segundos.
- `State_PKJSYS_Add/Rem` em `skilleffect.lua` soma/remove 30 pontos de STA e
  recalcula atributos derivados.
- A variável histórica do estado é `STATE_PKJSYS = 41`.
- A aplicação usa `enumSSTATE_ADD_LARGER`; um estado ativo de mesmo nível não
  é substituído. O item é consumido mesmo quando o estado não foi renovado.
- O mesmo lote porta o Battle Potion (1855): `State_PKZDYS_Add/Rem` soma/remove
  150 de ataque por 150 s, estado 19.
- Também porta o Accurate Potion (1859): `State_PKJZYS_Add/Rem` soma/remove
  30 de acerto por 900 s, estado 44.
- O Bull Potion (1854) soma/remove 1000 de HP máximo por 1800 s via
  `State_PKMNYS_Add/Rem`, estado 17.

## Porte OpenGL

`Play::clickBag` conecta os callbacks exatos dos quatro itens. O estado 1857 usa
o cálculo temporário de STA/SP existente; 1855, 1859 e 1854 têm estados próprios
que alimentam os cálculos atuais de ataque, acerto e HP máximo. Cada bônus
expira pelo ciclo temporário existente. Nenhuma regra ou efeito de skill foi
alterado.

## Verificação e limites

- O registro `item_action_audit.py` classifica os quatro pares item/callback; o
  ledger mantém fontes e pendências de validação.
- `top_gl` compila após a alteração.
- O efeito temporário ainda é mantido em memória e não sobrevive ao reinício;
  interação no cliente e save/reload não foram exercitados, portanto continua
  em D1.
