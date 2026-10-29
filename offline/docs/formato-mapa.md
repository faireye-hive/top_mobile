# Formato do `.map` (MindPower)

Confirmado em `arquivo/fontes/Libraries/Util/include/MPMapDef.h`, `arquivo/fontes/Engine/src/MPMapData.cpp` e numa leitura real de `Client/map/room.map` e `Client/map/garner.map`.

## Cabeçalho

20 bytes, cinco `int32` little-endian, sem compactação especial:

| Campo | Exemplo em `garner.map` |
|---|---|
| `nMapFlag` | 780627 (`MP_MAP_FLAG + 3`, com `NEW_VERSION`) |
| `nWidth` | 4096 tiles |
| `nHeight` | 4096 tiles |
| `nSectionWidth` | 8 |
| `nSectionHeight` | 8 |

`MP_MAP_FLAG` vale 780624. Flag 780625 é versão velha e o client recusa. Flag esperada com `NEW_VERSION`: 780627.

## Índice de seções

Logo depois do cabeçalho: `nSectionCnt` vezes `uint32`. `nSectionCnt = ceil(width/8) * ceil(height/8)`. Em Garner são 262144 offsets, cerca de 1 MB.

O offset é absoluto no arquivo. Zero significa seção vazia (mar sem tile, ou buraco). A seção ocupa `8 * 8 * 15 = 960` bytes.

`room.map` tem 52×52 e usa seções parciais nas bordas (a tabela de offsets arredonda para 7×7). `garner.map` tem cerca de 51 MB. `lobby.map` tem 128×128.

## Tile novo (`SNewFileTile`), 15 bytes, pack 1

Definido com `#pragma pack(1)`:

| Offset | Tipo | Uso |
|---|---|---|
| 0 | uint32 | textura das layers 1–3 compactada (`TileInfo_8To5`) |
| 4 | uint8 | textura da layer 0 |
| 5 | int16 | cor RGB565 |
| 7 | int8 | altura: metros = valor × 0.1 |
| 8 | int16 | região |
| 10 | uint8 | ilha |
| 11 | uint8[4] | quatro sub-células de bloqueio |

A altura do código original é `(float)(tile.cHeight * 10) / 100.0f`, ou seja, `cHeight * 0.1`. Nível do mar é 0 (`SEA_LEVEL` em `MPTile.h`). `room.map` decodifica tudo em 0.5 m. Água de verdade fica em altura ≤ 0.

## Bloqueio

Cada tile cobre 100 cm (1 tile = 1 metro no sistema de coordenada do servidor). Os quatro bytes são um grid 2×2, 50 cm cada. O bit 128 marca parede (`MPTile::IsBlock` e `ZRBlockData::IsBlock`).

Ordem usada em `arquivo/fontes/Engine/src/MPMap.cpp` ao preencher o grid:

- byte 0: sub-célula (0, 0)
- byte 1: (1, 0)
- byte 2: (0, 1)
- byte 3: (1, 1)

Coordenada de mundo do spawn e do NPC está em centímetros. `garnerChaSpn.txt` usa `215000,252800`. O ponto de nascimento em `birth_conf.lua` está em tiles: `AddBirthPoint("Argent City", "garner", 2232, 2787)` = 223200 cm, 278700 cm.

Índice da sub-célula para um ponto `(x_cm, y_cm)`:

```
gx = x_cm / 50
gy = y_cm / 50
byte = (gy % 2) * 2 + (gx % 2)
```

O runtime atual consulta os bits de região LAND (1) ou BRIDGE (8) para movimento terrestre, além do bloqueio de 50 cm. Não decide pela altura. Seção ausente e posição fora do mapa são bloqueadas; barcos usam sailableAt, que também rejeita seção ausente. A divergência do mar em seções vazias está documentada em [Estado do porte](../../sources/ESTADO_DO_PORTE.md).

## O que o `.map` não traz

Não traz o modelo dos prédios (isso é `map/*.obj` e `model/scene/*.lmo`) nem a textura do chão pronta para WebGL. O minimapa de Garner está em `Client/texture/minimap/garner/garner.pk`, formato proprietário, não BMP. O client OpenGL lê o terreno deste `.map` e coloca os prédios a partir do `.obj` e do `.lmo`.

## Referências históricas das tabelas

- `arquivo/servidor/server/GameServer/resource/mapinfo.txt` — id, arquivo, nome, coordenada de entrada.
- `arquivo/servidor/server/GameServer/resource/CharacterInfo.txt` — 125 colunas separadas por tab. O tipo lógico deve ser interpretado conforme a tabela histórica. Vários têm `Minimum Attack` e `Maximum Attack` em 0. O servidor preenche isso no Lua.
- `arquivo/servidor/server/GameServer/resource/<mapa>/<mapa>ChaSpn.txt` — retângulo em cm, ids de monstro, quantidade, `RFTime` em segundos.
- `arquivo/servidor/server/GameServer/resource/<mapa>/<mapa>npc.txt` — nome, id de aparência, coordenada em cm.
- `arquivo/servidor/server/GameServer/resource/script/birth/birth_conf.lua` — cidades e portos.
- `arquivo/servidor/server/GameServer/resource/script/calculate/skilleffect.lua`, função `Phy_Dmg`.

Velocidade: `MoveAble.cpp` faz `distância = tempo_ms * ATTR_MSPD / 1000`. A posição do protocolo é em centímetros, então `Movement Speed` 200 é 200 cm/s (2 tiles por segundo). `Attack Speed` nos monstros amostrados (caracol 2000) é tratado como intervalo em milissegundos.
