# Migração 05-CL — Blessed Potion

## Regra histórica

- Item 1860 referencia a skill 444 (`Blessed Potion`) e declara que evita dano
  por 5 segundos.
- `skillinfo.txt` liga a skill a `Skill_Wudiyaoshui_End`; o callback consome o
  item 1860 e aplica `STATE_PKWD` nível 10 por 5 s. O estado histórico tem ID 47.
- Os callbacks `State_PKWd_Add/Rem` estão vazios; a invulnerabilidade vem do
  estado especial do motor, não de uma alteração de atributos.

## Porte OpenGL

O uso do item cria um estado temporário de 5 s e consome um exemplar. Durante o
estado, o dano direto recebido e os ticks de veneno não reduzem HP. A regra foi
ligada ao item sem alterar o executor nem os efeitos de skills existentes.

## Verificação e limites

- O registro 1860 é classificado pela auditoria dos itens e relacionado à
  skill/estado históricos.
- Estado D1: compilação, uso interativo, compatibilidade de estados simultâneos
  e save/reload ainda precisam de validação. A duração é local à sessão.
