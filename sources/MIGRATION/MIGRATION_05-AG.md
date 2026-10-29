# Tales of Pirates — migração 05-AG: compatibilidade de caminhos do cliente Windows

Base: 05-AF. Escopo: cliente offline Linux/OpenGL.

## Origem da mudança

`05AF_assets.json` enviado pelo usuário: 53 mapas catalogados, 52 cabeçalhos válidos, apenas `binglang2.map` ausente; 13 caminhos com caixa incompatível. O auditador os encontra, mas `findMap()` e leitores de NPC/ChaSpn/portais abriam literalmente. O SceneWorld já tinha fallback de nome para `.obj` e foi preservado.

## Alterações

- `HistoricalPaths.h`: resolução exata prioritária, fallback case-insensitive por segmento, sem travessia `..`, rejeição de nomes ambíguos. Nunca escreve nos recursos.
- `main.cpp`: `findMap()` agora encontra `DreamIsland.map`, `PKmap.map` e `sdBoss.map` mesmo quando solicitado em minúsculas.
- `NpcSystem.cpp`, `MonsterSpawnSystem.cpp`, `PortalSystem.cpp`: arquivos de dados por mapa aceitam os nomes físicos com caixa distinta, preservando a preferência por `scripts/offline/maps/`.
- `MonsterSpawnSystem.cpp`: a seleção de mapas em `SetCurMap(...)` também compara os nomes sem diferenciar maiúsculas/minúsculas; não executa Lua.
- `UI05AGTests.cpp`: verifica resolução de mapas, leitura de NPC/portal/ChaSpn/Lua CreateCha, bloqueio de travessia e ambiguidade.
- Sem alterações de saves, renderização, equipamentos, forja, IA ou regras de combate.

## Observações do inventário

- `binglang2.map` continua ausente; só o cliente histórico com esse arquivo poderá recuperá-lo. Não inventamos geometria.
- 88 recursos opcionais ausentes incluem scripts de mapas de eventos/PvP, não 88 falhas de carregamento.
- 606 de 1182 referencias `.lmo` catalogadas faltam na instalação, mas o catálogo inclui itens que talvez nenhum mapa use. A prioridade futura é cruzar `.obj` e IDs de sceneobjinfo para determinar impacto real.
- `mapinfo.txt` é opcional e está ausente; a posição inicial cai no fallback já existente.
- Os 46 testes de 05-AF já passaram no Linux do usuário. Nesta versão, **47/47 testes** passaram neste ambiente (incluindo o novo teste da 05-AG). O executável OpenGL completo ainda precisa da validação no Ubuntu do usuário.

## Executar

```bash
cmake -S sources -B build -DTOP_BUILD_UI_TESTS=ON
cmake --build build --target top_gl top_regression_tests -j4
ctest --test-dir build --output-on-failure
./build/top_gl --check --map dreamisland
./build/top_gl --check --map pkmap
./build/top_gl --check --map sdboss
```

`--check` pode reprovar por outros recursos/habilidades do mapa: a alteração desta etapa corrige **encontrar** os arquivos, não garante jogabilidade ou execução de scripts em qualquer mapa.
