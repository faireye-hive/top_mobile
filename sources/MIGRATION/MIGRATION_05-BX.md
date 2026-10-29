# Porte 05-BX — promoção de primeira classe

O atalho da ficha já permitia escolher uma classe, mas as regras divergiam do
callback original `ClassChange(Type=1)` em `Client/scripts/offline/maps/script/MisSdk/NpcSdk.lua`.
Agora a promoção inicial exige nível 9 e 2.500 ouro, cobra a taxa somente ao
concluir, é definitiva e aplica as restrições históricas de raça:

- Swordsman: Lance, Carsise ou Phyllis; Ami não pode.
- Hunter: Lance ou Phyllis.
- Explorer: Lance, Phyllis ou Ami.
- Herbalist: Phyllis ou Ami.

O teste cobre nível mínimo, falta de ouro sem cobrança parcial, taxa exata,
impossibilidade de trocar novamente, Phyllis como Swordsman e Ami como Herbalist.
O `ClassChange` também contém promoção de segunda classe (nível 40/50.000 ouro),
mas ela permanece separada: as classes avançadas e suas missões de promoção
precisam de uma etapa própria antes de serem expostas no seletor.

Fonte histórica consultada (mantida intacta):
`Client/scripts/offline/maps/script/MisSdk/NpcSdk.lua`, `ClassChange`.
