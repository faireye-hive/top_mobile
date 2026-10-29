# 05-BM — New Year Chest

Item 2307 (`ItemUse_XNBOX`) now follows its historical gates: character level
40 or higher and at least one free bag slot. It issues one quality-4 Pirate
Voucher using the callback's exact weighted ranges across item IDs 2240,
2237, 2239, 2241 and 2242–2245. The output records are all present in the
client table. The existing chest transaction preserves the box if a reward
cannot be inserted, then saves the result.

The offline RNG sequence differs from the historical Lua/server RNG, so the
distribution is preserved but the exact outcome for a given seed is not.

Evidence: `Client/scripts/offline/script/calculate/ItemEffect.lua`;
`Client/scripts/table/iteminfo.txt`; `sources/Engine/src/item/ItemChestRules.cpp`;
`sources/Engine/src/item/PlayItems.cpp`.

Coverage remains D1 pending runtime and save/reload verification.
