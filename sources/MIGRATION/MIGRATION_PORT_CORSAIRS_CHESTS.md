# Porte dos baús de equipamento e armas Corsairs

O dispatcher agora lê do `ItemEffect.lua` as tabelas estáticas de três
callbacks, sem interpretar/executar Lua:

- `Corsairs_EquipmentChest`: entrega todos os itens/quantidades/qualidades da
  caixa (27 IDs históricos).
- `Corsairs_WeaponChest`: entrega o item fixo da tabela (18 IDs).
- `Corsairs_WeaponChest2`: escolhe uniformemente um dos itens da lista (8 IDs).

Todos os três callbacks recusam uso no barco. Os mínimos de espaços livres
reproduzem as condições Lua: `EquipmentChest` exige um espaço por item; os
dois callbacks `WeaponChest` exigem mais de `Quantity` espaços. A retirada e a
entrega são simuladas em uma cópia do inventário; caixa, qualidade, missões e
save só são atualizados se todos os resultados puderem ser inseridos.

## Limitações marcadas

- O sorteio de `WeaponChest2` usa o gerador offline e não a mesma sequência de
  `math.random` do servidor.
- O parâmetro de qualidade é preservado, mas os atributos aleatórios adicionais
  de equipamentos `chInstance` ainda não foram reconstruídos.
- Se faltarem dados locais ou espaço, a caixa é preservada para não perder o
  item. Os scripts antigos não fazem um teste de espaço nos dois callbacks de
  arma antes de chamar `GiveItem`; portanto, essa recusa é uma proteção do
  cliente offline.

`Play::selfTest` valida a carga de 53 tabelas, a caixa 617 com três recompensas
de qualidade 22, a escolha determinística da primeira opção de 6350, recusa
sem espaço, recusa no barco e preservação das caixas. `item_action_audit.py`
classifica esses registros como `corsairs_chest_PORT_APPROX`.
