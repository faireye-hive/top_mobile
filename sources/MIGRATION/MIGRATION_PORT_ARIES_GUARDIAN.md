# PORT — Summon Scroll - Aries

Three fixed summon callbacks now create their historical monsters from `characterinfo.txt` combat, drop, and model data:

| Item | Callback | Monster | Refresh | Lifetime |
|---|---|---:|---:|---:|
| 2962 Summon Scroll - Aries | `SummonAries` | 1009 | 3600 s | 3600 s |
| 1014 Goddess Summon Scroll | `ItemUse_NSSN` | 857 | 3700 s | 3600 s |
| 1027 Blury Medical Report | `ItemUse_MHDYSD` | 858 | 190 s | 180 s |

Each summon appears at the player's current position with the historical 145-degree facing.

All three callbacks reject use while sailing and inside the existing safe-zone region flag. Both refusals preserve the item. It is consumed and saved only after the historical row and actor model can be loaded. The existing random monster summon (item 319) keeps its five-pool selection, five-summon limit, and 40-second lifetime.

Evidence: `Client/scripts/offline/script/calculate/ItemEffect.lua::SummonAries`, `::ItemUse_NSSN`, `::ItemUse_MHDYSD`, the three `iteminfo.txt` records, and `characterinfo.txt` rows 857, 858, and 1009. This is a source-based implementation; runtime appearance, combat, and save failure behavior have not been validated visually or in a game session.
