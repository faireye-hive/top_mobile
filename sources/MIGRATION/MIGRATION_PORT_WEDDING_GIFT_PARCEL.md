# PORT — Wedding Gift Parcel

## Regra histórica

`ItemEffect.lua::ItemUse_HQBOX` requires four free bag slots and uses `math.random(1, 100)`:

| Resultado | Item | Qualidade |
|---:|---:|---:|
| 1–20 | 1012 | 4 |
| 21–40 | 1016 | 4 |
| 41–59 | 333 | 4 |
| 60 | sem prêmio | — |
| 61–100 | 273 | 4 |

The gap at result 60 is literal in the Lua and remains a consumed parcel with no reward.

## Fluxo OpenGL

The client validates the slot and reward data before committing. It consumes the parcel and grants the selected item in one bag transaction, then saves. The offline generator does not reproduce Lua's RNG sequence.

Evidence: `Client/scripts/offline/script/calculate/ItemEffect.lua::ItemUse_HQBOX`; `iteminfo.txt` IDs 2907, 1012, 1016, 333, and 273.
