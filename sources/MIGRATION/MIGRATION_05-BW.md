# Porte 05-BW — estado Poisoned (Poison Dart)

O callback historico `Skill_Db_End` aplica `STATE_ZD` com duracao `5 + 4 * nivel`.
`State_Zd_Add` causa imediatamente `10 + 2 * nivel` de dano via `Hp_Endure_Dmg`;
`skilleff.txt` configura a repeticao do callback em intervalo de um segundo.
O caso especial de Garner2 aplica 320 de dano a monstros nao normais. A descricao
textual da linha 0087 em `skillinfo.txt` diverge da regra executavel: diz 17 s no
nivel 1, mas Lua calcula 9 s. O porte segue Lua e registra a divergencia para
revisao historica, sem substituir a logica por texto descritivo.

No cliente offline, o estado agora e aplicado quando Poison Dart acerta um monstro,
causa o pulso inicial e mantem nivel, duracao e proximo pulso no registro do alvo.
Pulsos periodicos ignoram defesa/resistencia como `Hp_Endure_Dmg` e podem finalizar
o monstro pelo caminho normal de morte, incluindo experiencia, progresso de missao,
loot e persistencia. O teste regressivo verifica o nivel/duracao/intervalo e o
primeiro pulso periodico de 12 HP.

O lado recebido tambem porta `Deathsoul Poison Dart` (ID 291): o callback antigo
passa `hpdmg` sem inicializa-lo, mas aplica `STATE_WLDB`; o callback do estado
causa 160 HP ao adicionar e `skilleff.txt` marca intervalo de 1 s por 10 s. O
cliente aplica esses 160 HP como dano imediato e continua o DOT no jogador, sem
inventar o dano direto indefinido do callback. O regressivo cobre aplicacao e
primeiro pulso.

Fontes historicas consultadas (mantidas intactas):

- `Client/scripts/offline/script/calculate/skilleffect.lua` — `Skill_Db_End` e `State_Zd_Add`.
- `Client/scripts/table/skilleff.txt` — estado 4, Poisoned, intervalo de 1 s.
- `Client/scripts/table/skillinfo.txt` — habilidade 0087 e descricao textual divergente.
- `arquivo/servidor/server/GameServer/resource/script/calculate/skilleffect.lua` —
  `Skill_wldb_End` e `State_wldb_Add` do monstro 0291.
- `arquivo/fontes/Server/GameServer/src/Character.cpp` — callback inicial ao adicionar estado.
