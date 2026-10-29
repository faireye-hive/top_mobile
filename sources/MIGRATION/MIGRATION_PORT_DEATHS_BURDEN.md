# PORT: Death's Burden A-H

## Regra historica

`Client/scripts/offline/script/calculate/ItemEffect.lua` defines callbacks `ItemUse_DathBagA` through `ItemUse_DathBagH` for item IDs 2919..2926. Each rejects use while sailing and requires at least two free bag slots. A-D/E-G roll `math.random(1,4)`: result 4 grants a class piece, and results 1..3 grant a common piece. Every grant uses quality 16. Callback H grants item 2930 on either branch.

| Item | Callback | 25% class piece | 75% common piece |
|---:|---|---:|---:|
| 2919 | `ItemUse_DathBagA` | 2846 | 2928 |
| 2920 | `ItemUse_DathBagB` | 2847 | 2929 |
| 2921 | `ItemUse_DathBagC` | 2848 | 2927 |
| 2922 | `ItemUse_DathBagD` | 2849 | 2927 |
| 2923 | `ItemUse_DathBagE` | 2850 | 2929 |
| 2924 | `ItemUse_DathBagF` | 2851 | 2931 |
| 2925 | `ItemUse_DathBagG` | 2852 | 2932 |
| 2926 | `ItemUse_DathBagH` | 2930 | 2930 |

## Fluxo OpenGL

`ItemChestRules` maps only the matching historical item/callback pairs. The item-use transaction checks the sailing restriction and historical free-slot threshold, resolves the output from local `iteminfo`, simulates consuming the bag and inserting the reward, preserves the bag on failure, and saves after success. The weighted offline roll reproduces 1/4 versus 3/4 probability but does not share Lua's RNG sequence.

## Cobertura

Classified `D1`: rules and results are transcribed from the historical Lua. Runtime use and save/reload remain to be verified. No skills, missions, or map data changed.
