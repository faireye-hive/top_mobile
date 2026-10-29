# PORT: Monster Summoning Scroll

## Fonte historica

`Client/scripts/offline/script/calculate/ItemEffect.lua::ItemUse_MonsterSummon` chooses one of five pools: `40..80`, `98..146`, `194..241`, `500..571`, or `789`. It creates the monster at the character position facing `145` degrees and calls `SetChaLifeTime(..., 40000)`. Item 319 registers this callback in `iteminfo.txt`.

## Implementacao OpenGL

- The rows in those pools are cached from `characterinfo.txt`, without adding them as normal map spawns.
- Using item 319 selects the same pool and range, builds combat, attack, and drop data from the selected historical row, and loads the model into the current ActorSkin roster on demand.
- The summon is temporary and is removed from combat after 40 seconds. The scroll is consumed only after the model and row are available; otherwise it remains in the bag.
- Summons reuse an already-loaded model when no current monster instance uses that model slot. If the same monster is already present, a separate ActorSkin body is loaded for the new instance.

## Cobertura e pendencias

This is `D1 / PORT_APPROX`: map `CreateCha` entries use their fifth argument as respawn seconds in `MonsterSpawnSystem`; the summon therefore uses a 50-second respawn delay, longer than its 40-second lifetime. The in-game visual/combat sequence has not been compared at runtime. The PORT-00 item auditor and `ITEM_ACTIONS_05-AW.csv` classify item 319 as `monster_summon_D1`. The implementation does not change skill effects, historical map spawns, or mission data.
