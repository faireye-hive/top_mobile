# Migração 05-CH — Chaos Chest

## Regra histórica

`Client/scripts/offline/script/calculate/ItemEffect.lua::ItemUse_FightingBox`
associa o item 1134 ao Chaos Chest. O callback verifica ao menos um espaço
livre, concede o Expert Chaos Voucher 2610 com qualidade 4 e sorteia uma peça
entre 1124–1127 com qualidade 15.

## Porte OpenGL

- `Engine/src/item/ItemChestRules.cpp`: cadastra o plano pelo par exato de ID e
  callback, preserva a verificação histórica de um espaço e transcreve as duas
  recompensas.
- `Engine/src/item/PlayItems.cpp::Play::clickBag`: usa o commit transacional
  existente. O inventário é simulado antes de consumir o baú; se os dois
  prêmios não couberem ou algum dado estiver ausente, nenhum estado é alterado.
- `Tools/item_action_audit.py` e `Tools/port_coverage_overrides.csv`: passam a
  registrar o item e o callback como portados em D1.

## Limites e verificação

- O sorteio local usa o RNG do cliente offline, portanto a sequência não é
  idêntica ao `math.random` do servidor.
- O plano garante que os dois resultados caibam antes do commit. Isso evita
  perda parcial de recompensa que poderia ocorrer no envio sequencial de
  `GiveItem` quando só há um espaço livre e o voucher ainda não está empilhado.
- O alvo `top_gl` compila. Não foi feita validação interativa nem save/reload
  deste baú; por isso a cobertura continua em D1.
