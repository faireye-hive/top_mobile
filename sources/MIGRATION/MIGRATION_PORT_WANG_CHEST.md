# PORT — Wang Xiao Hu's Chest (ID 7363)

`Corsairs_WangChest` now uses the historical level-based reward lists. Levels 50–55 receive one uniformly selected level-55 class belt (IDs 7294, 7297, 7300, 7303, 7306, 7309); levels 60 and higher receive one uniformly selected level-65 belt (IDs 7312, 7315, 7318, 7321, 7324, 7327). Rewards use quality 4.

The historical callback has no reward branch for levels below 50 or from 56 through 59. The offline client preserves the chest at those levels. A full inventory or unavailable reward also preserves it through the existing transactional chest flow. Local RNG sequence and the server-wide Notice are not reproduced; runtime and save/reload are unverified. These limits are marked `PORT_APPROX` in the PORT-00 ledger.
