# PORT — Fixed Aries and Taurus apparel chests

## Regras históricas

Both `ItemEffect.lua` callbacks require four free inventory slots and grant apparel at quality 4.

| Chest | Callback | Historical rewards |
|---:|---|---|
| 2955 Aries Captain Apparel Chest | `ItemUse_CZBox` | IDs 5525–5527; character type 4 also gets 5528 |
| 3035 Taurus Captain Apparel Chest | `ItemUse_JNCZBox` | Four-piece set: type 1 → 5356–5359, type 2 → 5360–5363, type 3 → 5364–5367, type 4 → 5368–5371 |

## Fluxo OpenGL

The client validates the character set and all reward records, simulates chest consumption and all grants, then commits the inventory and save together. A missing item, unsupported character type, or capacity failure preserves the chest. Existing item-acquisition tracking receives the granted IDs.

## Limite de cobertura

Lua branches on `GetChaTypeID`; the offline client maps that choice from character IDs 1–4. This mapping remains `PORT_APPROX` until confirmed against live character types.

Evidence: `Client/scripts/offline/script/calculate/ItemEffect.lua::ItemUse_CZBox` and `ItemUse_JNCZBox`; `iteminfo.txt` IDs 2955, 3035, and 5356–5371/5525–5528.
