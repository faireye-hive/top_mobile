# Auditoria de tickets históricos — 05-AY

**Origem:** `GameServer/resource/script/calculate/variable.lua` (`TicketSys.Ticket`), `script/birth/birth_conf.lua` (`AddBirthPoint`) e `script/calculate/ItemEffect.lua` (`Ticket`). Não confundir com `script/shop/tickets.lua`, que apenas lista pacotes vendidos.

A implementação utiliza os 45 pares válidos **ID → cidade → mapa/ponto**, com verificação adicional de `iteminfo` callback=`Ticket`. A versão offline seleciona o **primeiro** ponto de nascimento da cidade de maneira determinística. O servidor original tinha múltiplos pontos e pequena variação aleatória.

| ID | Cidade | Mapa | X | Y |
|---:|---|---|---:|---:|
| 332 | Spring Town | `darkblue` | 3323.0 | 2507.0 |
| 563 | Summer Island | `darkblue` | 3681.0 | 2653.0 |
| 583 | Autumn Island | `darkblue` | 2635.0 | 3004.0 |
| 2445 | Caribbean | `jialebi` | 620.0 | 951.0 |
| 2447 | Skeletar Isle | `jialebi` | 609.0 | 624.0 |
| 2491 | Naval Base | `jialebi` | 1084.0 | 206.0 |
| 2844 | Abaddon 4 | `jialebi` | 155.0 | 907.0 |
| 3048 | Thundoria Castle | `garner` | 736.0 | 1592.0 |
| 3049 | Thundoria Harbor | `garner` | 990.0 | 1263.0 |
| 3050 | Sacred Snow Mountain | `garner` | 914.0 | 1072.0 |
| 3051 | Andes Forest Haven | `garner` | 1007.0 | 2966.0 |
| 3052 | Oasis Haven | `magicsea` | 787.0 | 3121.0 |
| 3053 | Icespire Haven | `darkblue` | 2681.0 | 647.0 |
| 3054 | Lone Tower Entrance | `garner` | 271.0 | 1775.0 |
| 3055 | Barren Cavern Entrance | `magicsea` | 1369.0 | 3033.0 |
| 3056 | Abandoned Mine 2 | `eastgoaf` | 116.0 | 28.0 |
| 3057 | Silver Mine 2 | `eastgoaf` | 238.0 | 27.0 |
| 3058 | Silver Mine 3 | `eastgoaf` | 382.0 | 123.0 |
| 3059 | Lone Tower 2 | `lonetower` | 35.0 | 21.0 |
| 3060 | Lone Tower 3 | `lonetower` | 120.0 | 406.0 |
| 3070 | Lone Tower 4 | `lonetower` | 320.0 | 175.0 |
| 3071 | Lone Tower 5 | `lonetower` | 483.0 | 121.0 |
| 3072 | Lone Tower 6 | `lonetower` | 494.0 | 297.0 |
| 3073 | Abandon Mine 1 | `garner` | 1642.0 | 2698.0 |
| 3076 | Spring Town | `darkblue` | 3323.0 | 2507.0 |
| 3828 | Thundoria Castle | `garner` | 736.0 | 1592.0 |
| 3829 | Thundoria Harbor | `garner` | 990.0 | 1263.0 |
| 3830 | Sacred Snow Mountain | `garner` | 914.0 | 1072.0 |
| 3831 | Andes Forest Haven | `garner` | 1007.0 | 2966.0 |
| 3832 | Oasis Haven | `magicsea` | 787.0 | 3121.0 |
| 3833 | Icespire Haven | `darkblue` | 2681.0 | 647.0 |
| 3834 | Lone Tower Entrance | `garner` | 271.0 | 1775.0 |
| 3835 | Barren Cavern Entrance | `magicsea` | 1369.0 | 3033.0 |
| 3836 | Abandoned Mine 2 | `eastgoaf` | 116.0 | 28.0 |
| 3837 | Silver Mine 2 | `eastgoaf` | 238.0 | 27.0 |
| 3838 | Silver Mine 3 | `eastgoaf` | 382.0 | 123.0 |
| 3839 | Lone Tower 2 | `lonetower` | 35.0 | 21.0 |
| 3840 | Lone Tower 3 | `lonetower` | 120.0 | 406.0 |
| 3841 | Lone Tower 4 | `lonetower` | 320.0 | 175.0 |
| 3842 | Lone Tower 5 | `lonetower` | 483.0 | 121.0 |
| 3843 | Lone Tower 6 | `lonetower` | 494.0 | 297.0 |
| 3847 | Abandon Mine 1 | `garner` | 1642.0 | 2698.0 |
| 4602 | Argent City | `garner` | 2232.0 | 2787.0 |
| 4603 | Shaitan City | `magicsea` | 891.0 | 3575.0 |
| 4604 | Icicle Castle | `darkblue` | 1316.0 | 536.0 |

**Bloqueado:** ID 3141, destino vazio no catálogo original; IDs fora da tabela também continuam sem viagem. O arquivo `Client/` pode não possuir todos os mapas indicados; uma tentativa falha não consome o ticket.
