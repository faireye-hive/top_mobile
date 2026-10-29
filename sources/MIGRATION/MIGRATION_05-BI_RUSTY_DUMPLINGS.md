# 05-BI — Rusty dumplings

Ported the historical callbacks for items 3037, 3043, 3044 and 3045. Their
item-table descriptions still advertise experience, defense, movement speed or
HP recovery, but the authoritative server callbacks only print the same
“gone rusty” notice and do not call `UseItemFailed`. `CharacterCmd.cpp`
therefore treats each invocation as successful and removes one item.

The OpenGL path now displays that notice, consumes exactly one item and saves
the updated inventory. It intentionally does not apply the stale table
descriptions as gameplay effects. The four item actions and Lua callbacks are
recorded at D1 pending runtime/save-reload verification.

Evidence:

- `arquivo/servidor/server/GameServer/resource/script/calculate/ItemEffect.lua`,
  callbacks `ItemUse_WQZZ`, `ItemUse_DHZ`, `ItemUse_DSZ` and `ItemUse_NMZ`.
- `arquivo/fontes/Server/GameServer/src/CharacterCmd.cpp`, `Cmd_ItemUse`:
  `UseItemFailed` determines failure; otherwise one item is popped.
- `sources/Engine/src/item/PlayItems.cpp`, `Play::clickBag`.
