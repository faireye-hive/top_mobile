# Career Lot (3086)

The offline inventory action in `Engine/src/item/PlayItems.cpp` ports `ItemUse_SuanmingWork`, `Suanming_Work`, and `SuanmingTeshu_Work` from the historical Lua scripts.

The handler enforces the level-80 limit, positive current-level EXP, and the sailing restriction. It preserves normal and Mystic Clover outcome thresholds, EXP gains/losses, 90% current-HP damage, temporary double-EXP state, and the five random combat states (attack, defense, maximum HP, hit, and attack speed). EXP follows the historical overflow reduction and levels the character immediately, with the existing level-up rewards. Using the Clover consumes one Clover together with one lot. EXP and item consumption save immediately.

`PORT_APPROX`: the offline RNG sequence differs from Lua; historical absolute `CEXP` is represented as the local current-level EXP bar; temporary states are session-only. The lottery defense state is stored separately from skills and existing combat rules.

This port does not edit skill effects, add missions, or change map content.
