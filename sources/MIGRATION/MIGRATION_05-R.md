# Migracao 05-R — spawns reais por mapa (offline)

## Problema corrigido
Ate a 05-Q, `Platform/linux/main.cpp::actorRoster()` criava sempre monstros
`71, 75, 103, 119, 185` quando nao encontrava uma regiao de spawn proxima.
`ActorSkin::open()` possuia um segundo fallback com esses mesmos IDs. Como o
jogador costuma aparecer numa cidade depois de teleportar, os mapas exibiam
sempre os mesmos monstros iniciais, independentemente dos dados historicos.

## Mudancas
- **Nenhum monstro inventado**: sem `ChaSpn.txt` valido ou sem spawns no raio
  de streaming, a populacao de monstros fica vazia. NPCs continuam aparecendo.
- Carregamento isolado pelo nome real de cada mapa em
  `Client/scripts/offline/maps/<mapa>/<mapa>ChaSpn.txt` (mantido o caminho
  alternativo offline historico da 05-Q).
- Quantidades iguais a zero nao geram criaturas.
- Um exemplar de cada tipo/regiao tem prioridade antes das copias restantes;
  o limite global de monstros visuais locais agora e 20 e o raio 84 m.
- Posicoes permanecem dentro dos retangulos historicos e sao ancoradas em
  celulas de 8 m, reduzindo o efeito visual de monstros que seguem o jogador
  ao atualizar o streaming.
- Trocas de mapa atualizam a referencia de streaming imediatamente.
- `Play::bind` aceita mapas sem monstros, sem invalidar a sessao offline.
- `MonsterSpawnSystem::closestRegion` permite identificar a distancia ate a
  regiao de nascimento mais proxima.

## Diagnostico novo

```bash
./build/top_gl --spawns --map garner
./build/top_gl --spawns --map darkblue
./build/top_gl --spawns --map magicsea
./build/top_gl --spawns --map jialebi
# Para entrar diretamente perto dos monstros de uma regiao historica:
./build/top_gl --map darkblue --spawn-near
./build/top_gl --map magicsea --spawn-near
```

O comando lista apenas mapas com tabela de spawns historicos nao vazia,
mostra quantos monstros existem perto do ponto inicial do mapa selecionado
e informa a distancia ate a regiao de nascimento mais proxima.

A opcao `--spawn-near` inicia apenas esta execucao perto do centro da regiao
historica mais proxima, para inspecao; a posicao e ajustada ao terreno
transitavel. Nao altera a logica de teleporte do jogo normal.

Quando um mapa nao possui dados `ChaSpn` no pacote, o programa nao simula
uma populacao desconhecida. Exemplos do acervo de testes:
- `garner`: 160 regioes validas.
- `darkblue`: 172 regioes validas.
- `magicsea`: 86 regioes validas.
- `eastgoaf`: 4 regioes validas.
- `DreamIsland`: 21 regioes validas.
- `jialebi`: sem registros de spawn no `ChaSpn` fornecido.

O fato de uma tabela estar vazia NAO prova que o servidor original nao tinha
criaturas nessa area: algumas instancias dependem de scripts Lua, eventos ou
outras tabelas, que ainda nao sao executados pelo cliente offline.

## Testes

```bash
cmake -S sources -B build-ui-tests -DTOP_BUILD_UI_TESTS=ON
cmake --build build-ui-tests -j4
ctest --test-dir build-ui-tests --output-on-failure
```

Nova suite: `ui05r_historical_map_spawns`.

A suite cria arquivos `ChaSpn` sinteticos e testa: isolamento por mapa,
coordenadas no retangulo historico, zero-count, lista sem falsos spawns,
diversidade, estabilidade de posicao, distancia para spawn e valores invalidos.
Foi rodada adicionalmente contra os `ChaSpn` reais de Icicle e Shaitan.

A compilacao integral `top_gl` depende dos headers GL/X11 do Linux do usuario.
Nenhum arquivo de interface, skin de arma ou sistema de rede foi modificado.
