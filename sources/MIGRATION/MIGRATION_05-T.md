# Migração 05-T — persistência dos spawns e temporizadores CreateCha

**Pré-condição:** 05-S foi compilada no Linux e validada com 24/24 testes. O levantamento 05-S foi removido por estar superado; o diagnóstico atual está em [ESTADO_DO_PORTE.md](ESTADO_DO_PORTE.md).

## Problema observado na 05-S

O streaming local de monstros recria os actores perto do jogador a cada mudança de setor. `Play::rebindWorld()` reconstruía os monstros do zero, revivendo imediatamente inimigos derrotados. Enquanto vivos, os monstros com HP reduzido também podiam voltar ao HP máximo. `CreateCha(..., respawn)` já era lido, mas o combate ignorava esse intervalo e sempre ressuscitava em 8 segundos.

## Correções da 05-T

- `MonsterSpawnSystem::nearest()` atribui uma **chave de nascimento estável** por registro literal Lua ou por combinação `(região, espécie, cópia)` de ChaSpn. Dois monstros com o mesmo ID podem ter chaves diferentes.
- Os dados `spawnKey` e `respawnSeconds` chegam a `Play` através de `MonsterSlot` e `ActorSkin::monsterSlot`.
- `MonsterLifecycle.h` guarda HP, localização, direção e prazo de respawn por **mapa + chave de nascimento**; o estado é preservado em `rebindWorld()` e na troca de mapa por `setWorldMap()`.
- Os monstros Lua mortos ressurgem após seu tempo positivo registrado em `CreateCha`. Os que têm valor 0 e os registros de ChaSpn (sem cronômetro explícito usado pelo porte) preservam o fallback offline anterior de **8 segundos**.
- Um inimigo reaparece com HP completo no ponto de nascimento, mas somente após o prazo. A animação de morte continua desaparecendo visualmente depois de ~1,1 s, independentemente do tempo total de espera.
- Relógio **da simulação local**: só progride nos quadros em que `Play::step()` é chamado; não avança se o jogo for fechado, se o modo de barco não atualizar o sistema de combate ou se o modo local estiver pausado. Não é um cronômetro do servidor original.
- `--spawns --map <nome>` pode exibir registros `CreateCha` mesmo quando não existe um arquivo gráfico `<nome>.map`. Exemplo: `secretgarden`; o diagnóstico **não** torna um mapa gráfico ausente jogável.

## Segurança e limites

- Nenhuma expressão Lua é executada; só são usadas as chamadas literais já aceitas na 05-S.
- Sem persistência de respawn entre **reinicializações do executável**. Memória dos monstros é apenas em runtime, por mapa e personagem atualmente selecionado. Outro personagem não herda as mortes anteriores.
- O fluxo `bind()` ainda reconstrói alguns **estados temporários do jogador** quando a população é atualizada; a preservação destes estados está planejada para 05-U, pois é uma alteração separada.
- IDs de ChaSpn têm posição de nascimento **aproximada dentro da região**; a 05-T mantém a chave lógica quando o streaming recalcula essas posições, mas não torna essa posição equivalente ao servidor original.
- No acervo compartilhado `Client.zip`, alguns scripts de mapa têm spawns sem o respectivo asset `.map`. Não criamos mapas fictícios.

## Como validar no Linux

```bash
rm -rf build build-ui-tests
cmake -S sources -B build
cmake --build build -j4
./build/top_gl --check
./build/top_gl

cmake -S sources -B build-ui-tests -DTOP_BUILD_UI_TESTS=ON
cmake --build build-ui-tests -j4
ctest --test-dir build-ui-tests --output-on-failure

# A inspecao de scripts Lua nao exige o mapa grafico:
./build/top_gl --spawns --map secretgarden
```

**Critério de aceite funcional:** derrotar um monstro da região `garner` ou `magicsea`, caminhar 32 m para provocar o streaming, retornar antes do tempo de respawn e confirmar que ele **não** voltou vivo. Depois do prazo, a criatura deverá reaparecer. Testar duas criaturas do mesmo ID independentemente. A regressão CTest esperada é 25/25.

## Validação feita durante a criação

- `ui05t_persistent_lua_respawn`: intervalos, independência, duplicatas, isolamento por mapa, Chaspn e Lua.
- `ui05r_historical_map_spawns` e `ui05s_lua_scripted_map_spawns`: recompilados com `MonsterSpawnSystem.cpp` da 05-T.
- `Play::selfTest()` executado com tabelas reais do `Client.zip`, incluindo casos 05-T adicionados no teste integrado.
- Sintaxe C++17 verificada em `Play.cpp`, `ActorSkin.cpp`, `MonsterSpawnSystem.cpp` e `main.cpp`.
- 25 suítes CTest passaram; **sete binários antigos não modificados** foram aproveitados da 05-S porque a recompilação total demorou demais. A validação completa do OpenGL e do comportamento real de respawn segue pendente no Linux do usuário.
