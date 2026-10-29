# Fortune Lot (3087)

The offline inventory action in `Engine/src/item/PlayItems.cpp` ports the Lua callbacks `ItemUse_SuanmingMoney`, `Suanming_Money`, and `SuanmingTeshu_Money` for Fortune Lot 3087.

It preserves the historical normal and Mystic Clover outcome thresholds and nested rolls, gold percentage/fixed changes, 90% current-HP damage outcomes, the 1092 scroll reward, the sailing rejection, and consumption of the lot and clover. The lot and clover are both preserved when the 1092 reward cannot be inserted into the bag. Successful results save the character state.

`PORT_APPROX`: OpenGL uses its existing drop-rate boost, which applies to local monster drops without the historical region boundary; the Lua/server RNG sequence is not reproduced; temporary boost duration is runtime-only and is not restored after reload. These gaps are explicit in `Tools/item_action_audit.py` and the generated item audit.

This port does not touch skill rules or effects, add missions, or change map content.
