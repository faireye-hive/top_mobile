# PORT — Fairy Coin Chest

The `ItemUse_YingbiBox` flow now follows the historical configuration: character level 41+, at least four free bag slots, and a coin cost of 20 plus the successful-open counter for the current day. The counter rises by five after each success. The chest consumes that many Fairy Coins, selects one active reward from `BaoXiang_CHESTCOIN` using its historical `Rad` weights, grants the reward at its configured quality, and returns a Fairy Coin Chest at quality 4.

The coin deduction, reward, returned chest, and daily counter are committed together after all catalog and capacity checks succeed. The counter is stored in the optional `FC1` save field. Older saves without the field start with zero opens for the day. The local implementation uses character identity and the machine's local date instead of the server's account PlayerID and server date; the random sequence is also local, so this feature is classified as `PORT_APPROX`.

Evidence: `Client/scripts/offline/script/calculate/ItemEffect.lua::ItemUse_YingbiBox` and `GiveBragiItem`, `variable.lua::CoinChest.Conf`, and `variable.lua::BaoXiang_CHESTCOIN`.
