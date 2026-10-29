# Migração 05-CJ — Berserk Potion

## Regra histórica

- O item 1856 chama `ItemUse_PKKBYS`, que aplica `STATE_PKKBYS` no nível 10 por
  20 segundos.
- `State_PKKBYS_Add/Rem` soma/remove 140 em `ATTR_STATEV_ASPD`, isto é, no
  estágio aditivo final de `Aspd_final`; o mesmo nível ativo não é renovado.
- `Aspd_final` aumenta a taxa histórica de ataque. No OpenGL, o bônus entra no
  cálculo do intervalo de ataque e termina pelo ciclo existente de estados
  temporários.

## Porte e limite

O callback de inventário consome o item, aplica o estado uma vez por janela e
recalcula `player_.cd`. A conversão do atributo histórico para a escala do
intervalo OpenGL é `PORT_APPROX`: o projeto não mantém toda a cadeia de atributos
ASP D do servidor. O tempo do estado continua local à sessão e não sobrevive ao
reinício.

## Evidência

- Fonte histórica: `ItemEffect.lua::ItemUse_PKKBYS`,
  `skilleffect.lua::State_PKKBYS_Add/Rem` e `functions.lua::Aspd_final`.
- Implementação: `Engine/src/item/PlayItems.cpp::Play::clickBag` e
  `Engine/src/top/play/PlayProgression.cpp::Play::applyAttrs`.
- Estado: D1; compilação e fluxo no jogo/save-reload ainda precisam de
  verificação.
