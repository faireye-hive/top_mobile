# Tales of Pirates — Migração 05-AK.3: índice único para scripts NPC

## Diagnóstico baseado em medição real

O `--startup-profile` da 05-AK.2 executado no Ubuntu identificou **2.725,11 ms até o primeiro quadro**, incluindo:

| Etapa | Tempo |
|---|---:|
| Indicadores de missões `!` e `?` — consulta Lua | **1.545,99 ms** |
| Play: save, inventário e combate | 463,19 ms |
| Comandos OpenGL do primeiro quadro | 188,96 ms |
| Personagens, modelos e animações | 160,32 ms |
| Animação dos atores do primeiro quadro | 114,05 ms |

O cache da 05-AK.1 já reduzia a releitura dos **MissionScript*.lua**. A demora remanescente está ligada principalmente a `NpcSystem::readScriptData()`, que continuava **tokenizando os arquivos NpcScript*.lua repetidamente** para cada função de NPC consultada na preparação dos marcadores.

## Correção

- Nova função `readNpcLuaDataIndex()` em `Engine/include/top/NpcLuaData.h`: tokeniza cada `NpcScriptXX.lua` uma vez e extrai passivamente as funções Lua de nível superior, preservando talk, loja e IDs de missões.
- `NpcSystem::readScriptData()` usa um único índice de funções carregado sob demanda para cada `NpcSystem`. Evita repetição de leitura de arquivos e tokenização. Resultados negativos também deixam de disparar varreduras repetidas.
- `NpcSystem::load()` invalida o índice, permitindo recarregar mudanças nos scripts ao trocar/recarregar mapas.
- Conserva precedência do primeiro arquivo quando a mesma função existe em múltiplos arquivos e rejeita funções sem fechamento.
- Novo teste `ui05ak3_single_pass_npc_lua_catalog` valida correspondência com o leitor antigo, comentários, funções aninhadas, loja, diálogo, IDs, duplicações e recarga.
- Identificação do executável atualizada para **05-AK.3**. `--startup-profile` e `--startup-report` da 05-AK.2 foram preservados.

**Não houve alterações em formato de saves, regras de missões, itens, equipamento, renderização ou dados `Client/`.**

## Medições locais de referência (não são resultados do Ubuntu do usuário)

No teste local, consultando 128 funções de NPC e 650 IDs distintos de missão em dados históricos de Garner/Argent City:

| Medida | Antes (05-AK.2) | Depois (05-AK.3) |
|---|---:|---:|
| Tempo de leitura dos scripts NPC | **366 ms** | **8 ms** |
| Funções de NPC consultadas | 128 | 128 |
| IDs de missão únicos | 650 | 650 |
| Missões válidas encontradas | 319 | 319 |
| Checksum dos IDs consultados | 10889424814280553822 | 10889424814280553822 |

A medição acima não inclui todos os custos de inicialização. **É necessário rodar `--startup-profile` novamente no Ubuntu para verificar o ganho real.** O tempo de `Play: save, inventário e combate` (463 ms no log anterior) permanece como candidato secundário à otimização.

## Verificações

- **54/54 testes de regressão headless aprovados** com os scripts históricos disponíveis.
- Sintaxe C++17 verificada para `NpcSystem.cpp` e `Platform/linux/main.cpp`.
- Compilação integral e execução gráfica da 05-AK.3 deverão ser realizadas no Ubuntu do usuário.
- `PROMPT_PROXIMA_MIGRACAO.md` atualizado.

## Testar no Ubuntu

Faça backup de `Client/user/` por segurança, e preserve `Client/` ao extrair o pacote:

```bash
cmake -S sources -B build -DTOP_BUILD_UI_TESTS=ON
cmake --build build -j4
ctest --test-dir build --output-on-failure
./build/top_gl --startup-profile 2>&1 | tee startup_terminal_05AK3.log
```

O comando gera `startup_profile.csv`. Compare a etapa `indicadores de missao (! e ?) - consulta Lua` com **1.545,99 ms** da 05-AK.2. Se houver demora perceptível remanescente, envie o CSV/log para avaliar a segunda etapa `Play` e renderização do primeiro quadro antes de iniciar a 05-AL (múltiplas missões simultâneas).
