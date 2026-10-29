# SEA-03 — laboratorio F9 e persistencia offline

Base auditada: `sources_261507(1).zip`. Nao modifica `EffectSystem`, dummies,
particulas ou testes de efeitos. Arquivos DX9 da auditoria:
`Antigo(8).zip/fontes/Client/src/WorldScene.cpp`, `Character.cpp` e
`Antigo(8).zip/fontes/Engine/src/MPMap.cpp`.

## Comportamento original comprovado

- DX9 `CWorldScene::_Init`: o cliente offline/editor cria personagens usando
  `g_Config.m_ChaList[i].nX/nY * 100` em `CCharacter::setPos`.
- DX9 `CCharacter::_UpdateHeight`: divide as coordenadas internas por 100
  para obter unidades do mapa; a altura de criaturas SEA usa `sSeaHeight/100`.
  SEA-03 nao altera altura nem fxs.
- A persistencia adicional por perfil `TOPLOC1` pertence ao porte OpenGL;
  nao foi identificada como funcionalidade do DX9 neste trecho. Os arquivos
  `offline-save-ID.txt` do jogo nao sao reformatados.

## Diferencas comprovadas na base auditada

- DEVSEA-01 ja tinha F9, posicao e checkpoint, mas nao um comando para destino
  arbitrario `mapa X Y`.
- A troca de personagem sempre levava ao `birthMap`/`birthX`/`birthY`, mesmo
  com arquivo de localizacao valido.
- Apos restaurar modo barco no inicio, `loadSettings` executava
  `setBoatMode(false,boatId)` e desmontava o modo salvo.
- `travel()` guardava o mapa de partida mas dependia do timer de 5 segundos
  para salvar o destino, e `placeOnLand` arredondava o destino para o centro
  do tile (mesmo quando o ponto era transitavel).
- O checkpoint de outro mapa nao restaurava yaw.
- A busca de spawn mais proximo tentava andar ate mar como se fosse terra.

## Atalhos e acoes F9

1. **Marcar ponto de retorno**: `Client/user/offline-checkpoint-ID.txt`.
2. **Voltar ao ponto marcado**: conserva mapa, X, Y, yaw e modo barco; valida
   previamente o tile do `.map` de destino.
3. **Ir para agua navegavel**: procura agua no mapa atual perto do personagem,
   pela mascara historica e mapa carregado, inclusive em outras partes do
   mesmo mapa caso nao encontre nada perto; nao cria agua ou terreno.
4. **Ir a spawn historico de monstros**: se o centro for navegavel, usa barco.
5. **Ir ao NPC mais proximo**: seleciona somente NPC carregado historicamente.
6. **Ir ao inicio deste mapa**: origem `mapinfo.txt` com validacao normal.
7. **Salvar posicao agora**: `Client/user/offline-location-ID.txt`.
8. **Teleporte manual**: linha editavel `mapa X Y [terra|barco]`, Enter executa,
   Esc cancela. Campo sem sufixo escolhe terra caminhavel ou mar navegavel
   conforme o tile. O mapa precisa existir. Digite apenas X e Y quando a
   linha com nome do mapa ja estiver preenchida; para outro mapa, apague o
   prefixo e informe o nome desejado.
9. **Proximo mapa historico**: percorre os mapas `.map` descobertos no Client.
10. **Buscar spawn marinho**: verifica posicoes historicas ChaSpn e Lua
    em tiles navegaveis (nao fabrica monstros).
11. **Inspecionar terreno**: imprime coordenadas e mascara historica no log.
12. **Fechar painel**.

Autosave a cada ~5 segundos e ao encerrar normalmente; viagens entre mapas
salvam imediatamente o destino. O botao manual grava sem aguardar o timer.
`--map`, `--spawn-near` e `--check` mantem as precedencias ja existentes.

## Validar no Ubuntu

Aplique na raiz do repositorio (primeiro `git apply --check`):

```
git apply --check SEA03.patch
git apply SEA03.patch
cmake -S sources -B build -DCMAKE_BUILD_TYPE=Debug -DTOP_BUILD_UI_TESTS=ON
cmake --build build -j$(nproc)
ctest --test-dir build -R 'sea03_dev_teleport_parser_and_terrain|dev_world_location_persistence|historical_sea_character_height' --output-on-failure
./build/top_gl
```

Roteiro manual: inicie no mapa normal, F9 marque checkpoint; clique na busca
por agua navegavel, verifique que o painel indica BARCO e que se pode ver o
terreno/mascara; salve posicao; feche normalmente; reinicie SEM `--map` ou
`--spawn-near`, confira mapa e posicao; F9 volte ao checkpoint e reinicie
novamente. Para testar escolha de mapa, use o campo manual com coordenadas
que a mascara local aceite ou navegue pelos mapas descobertos.

## Limites

- Nao foi executado um jogo completo com Client/map e X11 nesta auditoria,
  que so recebeu fontes e scripts/user. O teste grafico precisa ser feito
  pelo dono do repositorio que possui os recursos `Client` completos.
- A prova de validade de um destino vem do `MapFile`, nao comprova que cada
  regiao de monstro e uma zona de combate naval (SEA-04 e SEA-07).
- A selecao do casco especifico usa configuracao offline existente; o arquivo
  de localizacao registra modo barco, nao dados completos do navio.

## SEA-03.1 — restauração inicial do casco e orientação F9

- `offline-settings.txt` passa a ser lido antes de `ActorSkin` ativar o modo
  barco restaurado por `TOPLOC1`. A primeira construção do casco recebe
  `loadedSettings.boatId`; não se tenta trocar só o ID com o casco já ativo.
- No laboratório F9, o teleporte manual entre mapas passa `fromDevLab=true`
  para preservar o `yaw` de origem ao construir `nextActor`. As demais viagens
  (`travel()` de portais, NPCs, tickets e atalhos) mantêm `yaw=0` como antes.
- A seleção do casco continua sendo global por instalação/personagens,
  já que `TOPLOC1` guarda apenas o modo embarcado. Não altera saves antigos.
- Teste novo: `sea031_hull_restore_and_lab_heading`. Validação visual:
  selecionar casco 4 no jogo, embarcar, gravar posição com F9, sair, reabrir
  sem parâmetros de spawn e conferir o visual; em seguida teleportar para
  outro mapa com F9 e conferir direção, comparando com viagem por portal.
