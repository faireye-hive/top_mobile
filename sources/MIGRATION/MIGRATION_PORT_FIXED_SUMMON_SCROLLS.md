# PORT — Fixed monster summon scrolls

## Regras históricas

Four `ItemEffect.lua` callbacks reject use while sailing and in a safe zone, then use `CreateChaX` at the player position with a 9,000-second refresh and 9,000,000-millisecond lifetime:

| Item | Callback | Monster |
|---:|---|---:|
| 3012 | `ItemUse_XYZSTL` | 786 |
| 3014 | `ItemUse_HDCZ` | 757 |
| 3015 | `ItemUse_HY` | 761 |
| 3016 | `ItemUse_HLHT` | 952 |

## Fluxo OpenGL

Each scroll now checks the same travel and safe-zone conditions, resolves the historical monster through the local monster catalog, creates it at the player position with the historical lifetime, then consumes and saves the scroll. A failed catalog/model creation preserves the item.

## Limites

The local summon actor uses the existing offline AI and combat-stat mapping. The online `CreateChaX` service's spawn scheduling and server-side ownership behavior are not available offline, so this is `PORT_APPROX` pending runtime comparison.

Evidence: `Client/scripts/offline/script/calculate/ItemEffect.lua::ItemUse_XYZSTL`, `ItemUse_HDCZ`, `ItemUse_HY`, and `ItemUse_HLHT`.
