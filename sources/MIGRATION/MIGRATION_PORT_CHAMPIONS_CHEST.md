# PORT — Champion's Chest (ID 460)

`ItemUse_YZheBOX` now reaches the shared transactional chest flow. It checks the historical eight-free-slot requirement, grants IDs 453–456 at quality 4, then selects one of the eight literal reward groups from `Client/scripts/offline/script/calculate/ItemEffect.lua`.

The offline random sequence is approximate. Historical iteminfo rows for IDs 5034–5038 are absent. If one of those missing IDs is selected, the transaction leaves the chest and inventory unchanged; no replacement reward is invented. This limitation is recorded as `PORT_APPROX` in the PORT-00 override ledger. Runtime interaction and save/reload have not been verified.

The `top_gl` target builds successfully after this change. No skill effects or mission data were changed.
