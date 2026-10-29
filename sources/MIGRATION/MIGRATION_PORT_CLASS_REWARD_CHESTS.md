# PORT — Class reward chests (IDs 9618–9620)

The ChestYmazes handler selects class-specific rewards from the historical `BaoXiang_*` tables in `Client/scripts/offline/maps/script/calculate/variable.lua`. Incantation (9618), Evanescence (9619) and Enigma (9620) now map the six advanced jobs to their matching tables. Reward weights, quantities and quality are parsed from the source tables rather than duplicated in C++.

Opening a chest requires a free inventory slot and commits transactionally. An unrecognized job or missing reward data leaves the chest untouched. Offline random sequence and server-wide `Notice` are not reproduced; runtime and save/reload remain unverified. PORT-00 records these limitations as `PORT_APPROX`.
