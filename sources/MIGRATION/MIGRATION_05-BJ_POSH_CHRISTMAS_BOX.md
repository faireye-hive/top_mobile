# 05-BJ — Posh Christmas Box

Item 2900 (`ItemUse_GWZHQ`) is now routed through the existing historical
chest transaction. The callback requires four free bag slots before use, then
grants one each of items 2888, 2889, 2890 and 2891 at quality 4. The matching
records exist in the client item table. The generic chest path preserves the
box if space or reward data is missing, and commits its consumption and all
four rewards together before saving.

Evidence: `Client/scripts/offline/script/calculate/ItemEffect.lua`;
`Client/scripts/table/iteminfo.txt`; `sources/Engine/src/item/ItemChestRules.cpp`;
`sources/Engine/src/item/PlayItems.cpp`.

Coverage remains D1 until runtime and save/reload verification.
