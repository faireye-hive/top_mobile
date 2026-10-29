# PORT — Lucky Piggy Chest

## Regra histórica

`ItemEffect.lua::ItemUse_XYPIGBOX` requires one free inventory slot and rolls `math.random(1, 100)`:

| Resultado | Item | Chance | Qualidade |
|---:|---:|---:|---:|
| 1–4 | 1012 | 4% | 4 |
| 5 | 1016 | 1% | 4 |
| 6–30 | 861 | 25% | 4 |
| 31–60 | 885 | 30% | 4 |
| 61–75 | 860 | 15% | 4 |
| 76–88 | 862 | 13% | 4 |
| 89–100 | 863 | 12% | 4 |

## Fluxo OpenGL

The client validates the one-slot requirement and selected catalog item, simulates consuming the chest and granting the reward, then commits the bag and save. It preserves the literal ranges and reward IDs. The local random sequence differs from Lua's.

Evidence: `Client/scripts/offline/script/calculate/ItemEffect.lua::ItemUse_XYPIGBOX`; `iteminfo.txt` IDs 2909, 1012, 1016, 861, 885, 860, 862, and 863.
