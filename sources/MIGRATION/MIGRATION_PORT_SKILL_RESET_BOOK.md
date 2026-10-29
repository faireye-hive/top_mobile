# PORT — Book for Regaining Ability

## Regra histórica

`Client/scripts/offline/script/calculate/ItemEffect.lua::ItemUse_XiDianBook` requires two free inventory slots and refuses use while sailing. It clears learned fight skills and returns their spent points. If any of skills 453–458 is learned, it also grants a class-specific rebirth skill book (job IDs 8, 9, 12, 13, 14, 16 map to 2957, 2956, 2961, 2959, 2958, 2960) and item 1572, then deducts two skill points.

## Fluxo OpenGL

Item 2216 now checks the boat and capacity restrictions before changing state, validates that every learned skill has a local rule, simulates consuming the book and granting all available historical returns, then clears combat skills and hotbar cooldowns, updates TP, saves, and cancels armed skill actions. Failures preserve the book and the character state.

## Limite de cobertura

The shipped `iteminfo.txt` has no record for item 1572. When a character has one of skills 453–458, the client therefore refuses before consuming the book rather than creating an unusable item or silently omitting a historical return. That rebirth branch remains `PORT_PARTIAL`. The local TP refund is computed from loaded skill costs and levels; `ClearAllFightSkill` itself is server-side and is not available for byte-for-byte parity.

Evidence: `ItemEffect.lua::ItemUse_XiDianBook`; `iteminfo.txt` item 2216 and the class-specific rebirth books 2956–2961.
