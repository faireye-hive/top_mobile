# 05-BG — GemVouchers

`Play::clickBag` now executes the six historical voucher tables defined by
`GemVouchers` in `Client/scripts/offline/script/calculate/ItemEffect.lua`.
The rule loader reads those literal reward lists instead of duplicating their
IDs in C++. The callback blocks use while sailing and requires one free bag
slot before rolling, as the Lua does. Voucher consumption, the quality-4
reward, the item mission event, and save happen only after the reward can be
inserted. A missing table/item or insufficient space leaves the voucher
untouched.

`PORT_APPROX`: the offline RNG has a different sequence from Lua/server RNG,
and the multiplayer-wide `Notice` is represented by a local player message.
Reward choices remain uniform over the exact historical list. This does not
port unrelated fairy skills, gem forging, or other callbacks.

Validation: `item_gem_voucher_historical_tables` checks all six table sizes
and representative first/last IDs; the `PlaySelfTest` integration path checks
boat/full-bag rejection, successful reward quality, consumption, and save/load.
