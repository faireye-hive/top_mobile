# Tales of Pirates — migração 05-V

Base: **05-U**. Objetivo: aproximar os atributos físicos históricos e impedir que
atualizações da população de monstros apaguem o estado temporário do jogador.
Sem rede, sem alterações no sistema de equipamentos 3D ou no layout das janelas.

## Regras históricas identificadas

Referências: `scripts/offline/calculate/skilleffect.lua` (`Phy_Dmg`,
`Check_MisorCrt`, `Mis_or_Crt`), `functions.lua` (`Crt`, `Resist`,
`CRTRADIX=100`) e `characterinfo.txt`.

* `characterinfo.txt`: **67 PDEF**, 68 DEF, 69 HIT, 70 FLEE, **71 CRT**.
* `Phy_Dmg`: para PDEF < 70, resistência = min(0,85, PDEF/100);
  caso contrário min(0,85, PDEF/150). Dano = floor(ATK * (1-resistência) - DEF).
  Mantemos mínimo de 1 de dano na simulação offline.
* `Check_MisorCrt`: chance crítica = min(1, CRT/100 + min(0,15,
  max(0, 0,03 * (nível atacante - nível defensor)))). Crítico usa o multiplicador
  2, conforme `Mis_or_Crt`.
* A checagem de esquiva ocorre antes da rolagem de crítico. O RNG é local, não
  depende do servidor.

Valores conferidos na tabela de monstros: Shadow Hunter (201) PDEF 27 e CRT 10;
Jungle Guardian (701) PDEF 31 e CRT 10. O `top_stats_inspect` mostra os campos.

## Streaming / salvamento

* `rebindWorld()` continua atualizando monstros por setor, mas conserva HP/SP
  (salvos), cooldowns de habilidades, temporizadores dos buffs atuais,
  cooldown de ataque e canhão, regeneração, seleção de habilidade,
  animação de conjuração, HP/estado de morte e posição sentada do personagem.
* Ataques pendentes com índice do monstro **não** são retomados: a população
  do novo setor pode ter outra ordem. Evita acertar o monstro errado.
* Novo campo opcional no **fim da linha** de save (após capacidade da bolsa):
  `|<SP_atual>`. Saves anteriores continuam legíveis. Salvar continua produzindo
  o formato original nas seções anteriores.
* Buffs e cooldowns são preservados **no streaming**, mas ainda **não** são
  persistidos ao fechar e reabrir o jogo. HP e SP são persistidos no save.

## Limitações

* Equipamentos continuam somando ATK/DEF estáticos do `iteminfo.txt`.
  Afinidades/atributos de instâncias equipadas que o servidor calcularia em
  Lua não são simulados sem dados equivalentes.
* PDEF/CRT do personagem usa a ficha base histórica. Evolução de atributos
  avançados/LUK, equipamento com PDEF/CRT, estados que modificam PDEF/CRT e
  a maioria das fórmulas especiais de habilidade ainda estão pendentes.
* Dano em área ainda reaproveita parte do dano calculado para o alvo principal;
  cada alvo não recebe uma fórmula mágica histórica separada.
* A 05-V não muda o mundo aberto, os arquivos de mapa nem o comportamento de
  IA e respawn da 05-T.

## Compilação Linux

```bash
rm -rf build build-ui-tests
cmake -S sources -B build
cmake --build build -j4
./build/top_gl --check
./build/top_stats_inspect Client 201 701
./build/top_gl
cmake -S sources -B build-ui-tests -DTOP_BUILD_UI_TESTS=ON
cmake --build build-ui-tests -j4
ctest --test-dir build-ui-tests --output-on-failure
```

Resultado esperado: 28 testes (26 anteriores + 2 da 05-V). Em ambiente sem
`GL/gl.h` a compilação do `top_gl` não é executável; os testes sem GL funcionam.
No ambiente de preparação, as 26 suítes anteriores reutilizaram binários da
05-U validados previamente; as duas suítes novas foram compiladas. Uma delas
executa `Play::selfTest()` do código 05-V diretamente. Verificada sintaxe C++17
em `Play.cpp`, `main.cpp` e inspetor. Execute a compilação integral no seu Linux.

## Observação: "Clock skew detected"

Alguns arquivos extraídos podem ter timestamps posteriores ao relógio local.
Isso não é um erro de compilador, mas pode prejudicar builds incrementais.
Nesta distribuição as datas dos arquivos no ZIP são normalizadas para o passado.
Se o aviso voltar, após verificar o relógio use `find sources -type f -exec touch {} +`
e refaça uma compilação limpa.
