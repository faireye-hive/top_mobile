# Taurus Guardian Scroll (3036)

The historical `ItemUse_JNSHZ` callback rejects use while sailing or in safe zones, calls `CreateChaX` with monster ID 1038, current coordinates, facing 145, and `Refresh=10900`, then sets a 3,600,000 ms lifetime.

The OpenGL inventory path reuses `createHistoricalSummon` and the local characterinfo row for ID 1038. It preserves the scroll if the creature row or local actor creation is unavailable and consumes/saves it only after creation succeeds. Existing fixed-summon scroll behavior remains unchanged.

`PORT_APPROX`: the offline summon system interprets Refresh through its existing respawn-seconds field. The game server's exact `CreateChaX` timing semantics and the creature's appearance/behavior need runtime comparison.

No mission, skill effect, or map data was changed.
