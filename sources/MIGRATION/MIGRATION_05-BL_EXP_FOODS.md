# 05-BL — Turkey meat and mooncake EXP

Ported the historical `ItemUse_MWHJ` (item 2895) and `ItemUse_YB` (item
3915) branches. Tasty Turkey Meat grants 5,000 EXP below level 80 and 100 at
level 80 or above. Mooncake grants 300 below level 80 and 6 at level 80 or
above. A successful use consumes one item and saves the character.

The historical callbacks write `ATTR_CEXP` directly with `SetCharaAttr`,
which calls `CountLevel`; these are intentionally implemented as direct EXP
grants through the existing offline experience table, rather than passing
through the server's separate `AddExp` scaling (which divides high-level
combat EXP by 50).

Evidence: `Client/scripts/offline/script/calculate/ItemEffect.lua`;
`arquivo/fontes/Server/GameServer/src/Expand.h`;
`arquivo/fontes/Server/GameServer/src/FightAble.cpp`;
`sources/Engine/src/top/play/PlayEconomy.cpp`.

Coverage remains D1 pending runtime and save/reload verification.
