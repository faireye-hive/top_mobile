# PORT — Rightful Chest of Black Dragon

## Regra histórica

`Client/scripts/offline/script/calculate/ItemEffect.lua::ItemUse_Zhenheilong` rejects use at sea and requires five free inventory slots. It grants IDs 2367, 2368, and 2369 at quality 16; character type 4 also receives ID 2370 at quality 16.

## Fluxo OpenGL

Item 2843 now checks the sailing and five-slot requirements, validates the full reward set against `iteminfo.txt`, simulates consuming the chest and granting all pieces, then commits the inventory and save together. Each reward is also reported to existing item-acquisition tracking.

## Limite de cobertura

The offline character catalog exposes `player_.id`, where 4 is Ami, while Lua checks `GetChaTypeID(role) == 4`. The extra-head branch maps that value to character ID 4 and remains `PORT_APPROX` until the runtime type mapping is confirmed.

Evidence: `ItemEffect.lua::ItemUse_Zhenheilong`; `iteminfo.txt` IDs 2843 and 2367–2370.
