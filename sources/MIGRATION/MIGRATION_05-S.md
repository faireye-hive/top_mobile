# Migração 05-S — monstros históricos em Lua (CreateCha sem executar Lua)

## Problema observado

O `ChaSpn.txt` não é a fonte única de monstros. No pacote histórico,
`Client/scripts/offline/script/monster/mlist.lua` seleciona scripts individuais
por mapa (`SetCurMap`, `PM`); esses scripts descrevem pontos de nascimento com
`CreateCha(id, x_cm, y_cm, facing_graus, respawn_segundos)`.

A 05-R mantinha as regiões `ChaSpn`, mas não carregava os pontos Lua. Em mapas
com `ChaSpn` ausente, muitos monstros não apareciam. Também havia sobreposição
quando a posição aproximada para uma região inteira caía fora de sua borda;
o `clamp` fixava várias criaturas numa mesma coordenada (ex.: 13,10 em eastgoaf).

## Comportamento da 05-S

- Carrega apenas os nomes listados por chamadas **literais** `PM("...")`
  dentro do bloco do mapa em `mlist.lua`. Scripts comentados e chamadas dentro
  de condicionais aninhadas não são incluídos por engano (eventos sazonais).
- Faz leitura **estática** de chamadas literais `CreateCha` com cinco inteiros;
  não executa `dofile`, nem funções do servidor, nem código Lua arbitrário.
- Preserva ID, posição (centímetros para metros), direção em graus, tempo
  de respawn e identificação do script-fonte; rejeita registros inválidos,
  duplicados exatos, nomes de arquivo inseguros e limites excessivos.
- `nearest` combina os pontos Lua exatos com as regiões `ChaSpn`: primeiro os
  monstros Lua próximos, depois exemplares de `ChaSpn` até o limite local de
  20. Ao carregar outra área, todas as listas antigas são descartadas.
- O carregador `Play` converte a direção inicial Lua de graus em radianos.
- O posicionamento complementar de `ChaSpn` espalha os pontos junto à borda,
  evitando vários monstros sobrepostos na mesma coordenada de limite.
- `--spawn-near` pode encontrar um ponto Lua mesmo quando não existe
  `ChaSpn` no mapa; o procedimento ainda ajusta o local ao terreno caminhável.
- `--spawns` e `--check` mostram separadamente regiões `ChaSpn`, total de
  `CreateCha` e número de scripts usados; a listagem inclui o script-fonte.

## Amostra do acervo Client.zip analisado nesta etapa

| Mapa | Scripts Lua ativos | Pontos CreateCha |
| --- | ---: | ---: |
| garner | 8 | 144 |
| magicsea | 15 | 1072 |
| darkblue | 8 | 1509 |
| eastgoaf | 2 | 361 |
| secretgarden | 3 | 71 |

Esses números refletem apenas os arquivos presentes no `Client.zip` usado
na validação. Outros pacotes podem ter versões ou quantidades diferentes.

## Testar no Linux

```bash
cmake -S sources -B build
cmake --build build -j4
./build/top_gl --check
./build/top_gl --spawns --map eastgoaf
./build/top_gl --spawns --map magicsea
./build/top_gl --spawns --map secretgarden
./build/top_gl --map secretgarden --spawn-near
./build/top_gl

cmake -S sources -B build-ui-tests -DTOP_BUILD_UI_TESTS=ON
cmake --build build-ui-tests -j4
ctest --test-dir build-ui-tests --output-on-failure
```

Teste novo: `ui05s_lua_scripted_map_spawns`, além das 23 suítes anteriores.
O teste cobre seleção de scripts por mapa, exclusão de comentários/eventos,
validação literal, isolamento de mapa, reprodução de coordenadas e prevenção de
sobreposição na borda; foi validado também com ASan/UBSan.

## Limites atuais

- **Não é um interpretador Lua**: não executa eventos, missões nem lógica
  condicional ou dinâmica do servidor. Isso evita ativar eventos históricos
  fora de época e executar código arbitrário no cliente offline.
- A coluna de respawn de `CreateCha` é preservada, mas a simulação de morte,
  despawn e ressurgimento continua governada pela lógica offline existente;
  não há temporizador Lua completo nesta etapa.
- A visão/IA/rastro de monstros e a navegação local continuam sendo as das
  versões 05-M a 05-Q, com algumas aproximações em relação ao servidor.
- A validação OpenGL em uma janela real depende do ambiente Linux do usuário.

Não houve alterações no layout da UI, redes, equipamentos ou modelos 3D.
