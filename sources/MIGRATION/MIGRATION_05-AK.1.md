# Tales of Pirates — Migração 05-AK.1 (hotfix de inicialização)

## Sintoma relatado
Ao abrir `./build/top_gl` depois da 05-AK, a janela demora e parece travar por alguns segundos antes de desenhar o jogo. O usuário confirmou 51/51 testes aprovados, `--check` e `primeiro quadro desenhado`. O banner `05-AI` era um texto de identificação desatualizado, não necessariamente sinal de build antiga.

## Causa reproduzida
A rotina `refreshQuestNpcMarkers()` (Platform/linux/main.cpp), adicionada na 05-AK, percorre os NPCs do mapa e consulta `NpcSystem::mission(id, ...)` para construir os sinais `!` e `?`. Antes da correção, **cada chamada** a `mission()` abria e varria os oito arquivos históricos `MissionScript01.lua` a `MissionScript08.lua`, mesmo para IDs inexistentes. A lógica tem custo proporcional ao número de NPCs vezes o tamanho total dos scripts.

Benchmark reproduzido com os scripts reais disponíveis de `Client/scripts/offline` em `garner`, build `g++ -std=c++17 -O2`, sem OpenGL: 153 NPCs, 652 consultas, 320 resultados válidos, **2,652 s** antes / **0,420 s** depois (~84% menos tempo no trecho). Medições não incluem criação da janela, renderização, carregamento de modelos e texturas; tempos podem variar no Ubuntu do usuário.

## Correção
- `NpcSystem` constrói um índice preguiçoso ID → (arquivo Lua, deslocamento do início de `DefineMission`). Os oito arquivos passam a ser varridos só uma vez, na primeira consulta, em vez de centenas de vezes.
- Um cache por ID evita reprocessar missões já abertas. IDs inexistentes são resolvidos por consulta ao índice, sem nova varredura de disco.
- Leitura de cada missão preserva o parser histórico atual (`MisBeginTalk`, `MisNeed`, recompensas, `AlwaysFailure`, `COMPLETE_SHOW`), incluindo a precedência dos arquivos.
- `NpcSystem::load` invalida o índice e o cache, permitindo recarregar dados ao mudar mapas ou atualizar a instalação offline.
- Atualizado banner da migração no `Platform/linux/main.cpp` para `05-AK.1`.
- Acrescentado teste automatizado `ui05ak1_cached_mission_index_and_reload`, em `tests/UI05AK1Tests.cpp`, com IDs repetidos, bloqueio, títulos/recompensas, casos `COMPLETE_SHOW`, recarga.

Nenhuma alteração no layout das janelas, formato do save, modelos, armas, forja ou combate.

## Validações
- Benchmark C++17 com scripts reais: 652 consultas, 320 retornos válidos antes e depois; 2,652 s → 0,420 s neste ambiente.
- Compilação específica do componente e teste de NPCs/UI: aprovada.
- Suíte CTest headless completa: **52/52 testes aprovados**, tempo total de testes 0,35 s.
- `main.cpp` e `NpcSystem.cpp` compilados com `-fsyntax-only` C++17.
- Compilação gráfica integral `top_gl` e medição de abertura no Ubuntu: pendentes; este ambiente não tem GL/gl.h.

## Instalar e testar no Ubuntu
Faça backup de `Client/user/` e extraia o ZIP completo apenas nos fontes da raiz do projeto, preservando a pasta `Client/`. Ou aplique o patch 05-AK → 05-AK.1 na raiz do repositório.

```sh
cmake -S sources -B build -DTOP_BUILD_UI_TESTS=ON
cmake --build build -j4
ctest --test-dir build --output-on-failure
./build/top_gl --check
./build/top_gl
```

Observe o intervalo entre executar `top_gl`, aparecer a janela e surgir `primeiro quadro desenhado`. Se o travamento persistir, testar as etapas `scene.open`, `actors.open`, `refreshQuestNpcMarkers` e a primeira renderização com perfil de inicialização, evitando mudanças indiscriminadas de renderização.

## Próximo passo: 05-AL
Continuar a expansão das missões históricas offline, múltiplas missões simultâneas, save retrocompatível e indicadores/diário por missão. Antes, obter a validação do hotfix de inicialização no Ubuntu do usuário. Gerar sempre o `PROMPT_PROXIMA_MIGRACAO.md` atualizado.
