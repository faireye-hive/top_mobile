# 05-BK — Number Ticket

Item 2305 (`ItemUse_SZF`) now requires one free inventory slot and grants one
IPOD Voucher (2306) at quality 4. Both item records are present in the
historical client table, and the existing chest transaction validates the
reward before consuming the ticket, then saves the resulting inventory.

The Lua callback also sends a server-wide `Notice`. In the offline client this
is explicitly approximated by a local message; no claim is made that the
global announcement is reproduced.

Evidence: `Client/scripts/offline/script/calculate/ItemEffect.lua`;
`Client/scripts/table/iteminfo.txt`; `sources/Engine/src/item/ItemChestRules.cpp`;
`sources/Engine/src/item/PlayItems.cpp`.

Coverage remains D1 pending runtime and save/reload verification.
