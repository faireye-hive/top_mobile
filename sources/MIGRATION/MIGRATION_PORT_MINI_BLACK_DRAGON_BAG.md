# PORT — Mini Black Dragon Bag

`ItemUse_minibh` (item 1019) now grants one of the four historical Black Dragon prize vouchers at quality 4. Its 1..10000 roll is represented by weights 1 / 50 / 1600 / 8349 for item IDs 272 / 273 / 274 / 275. The bag requires one free slot; the bag and reward are committed transactionally, so a full inventory preserves it.

The client RNG does not reproduce the server's random sequence. Item 274 (Second Prize) is preserved as the historical voucher, but its apparel contents are still incomplete because item IDs 5034..5038 are absent from the local catalog. This change does not claim that prize path is fully ported.

Evidence: `Client/scripts/offline/script/calculate/ItemEffect.lua::ItemUse_minibh`, `Client/scripts/table/iteminfo.txt::1019`, and the callbacks for prize vouchers 272..275.
