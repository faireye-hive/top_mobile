# Tales of Pirates offline / OpenGL — migração 05-AE

Base: **05-AD**. Nesta etapa, o vínculo entre os menus Lua históricos dos NPCs e
as operações da bancada foi reforçado; não foram adicionados novos serviços ou
alteradas as regras de itens.

## Correções

- `NpcForgeServices.h`: substitui a leitura textual linha-a-linha por um **tokenizador
  passivo** (não executa Lua). Reconhece comentários `--` e `--[=[...]=]`,
  strings entre aspas e strings longas `[=[...]=]`, inclusive chamadas `Text()`
  divididas em várias linhas.
- Apenas `Text(..., OpenForge)`, `Text(..., OpenUnite)` e
  `Text(..., OpenMilling)` concedem acesso, **com o identificador como terceiro
  argumento independente**. Palavras que só aparecem em descrições, comentários,
  chamadas `Obj.Text`, funções aninhadas ou em nomes maiores não concedem serviço.
- O parser acompanha os blocos `function`, `if`, `do` e `repeat`/`until`. Um `end`
  de uma condição interna não encerra precocemente a função do NPC.
- `NpcSystem::forgeServices()` usa correspondência **exata da função** e distingue
  script ausente de script existente sem serviços, sem procurar permissão
  indevidamente em outro arquivo com função de nome semelhante.
- Arquivos com definição Lua truncada não concedem serviço.
- Nenhum campo do save muda; regras históricas de forge/combination/socket,
  custos, chance e confirmação por UID continuam inteiramente em `Play`.
- Atalho de desenvolvimento `Alt+B` permanece com as três abas. A limitação
  de NPC ao mesmo mapa e raio de 2,4 unidades permanece no `main.cpp`.

## Compilar

```bash
cmake -S sources -B build
cmake --build build -j4
./build/top_gl --check
```

### Testes sem exigir OpenGL

```bash
cmake -S sources -B build-ui-tests -DTOP_BUILD_UI_TESTS=ON
cmake --build build-ui-tests --target top_regression_tests -j4
ctest --test-dir build-ui-tests --output-on-failure
```

Novo teste `ui05ae_npc_lua_tokenizer_and_forge_service_isolation`. Os testes
05-AC, 05-AD e 05-AE passaram neste ambiente; o conjunto completo compilou
(45 targets de testes) e **44/45 passaram**. A única falha foi no
`ui05v_play_state_regression`, que depende de arquivos históricos de
`Client/scripts/table/skillinfo.txt` e `Client/scripts/offline/calculate/variable.lua`:
esses arquivos **não estão no ZIP de fontes disponibilizado**, portanto o
teste encontrou tabelas vazias (falha `skillinfo: IDs de trajeto visual 05-F`).
Para o conjunto de testes independente de `Client`, execute:

```bash
ctest --test-dir build-ui-tests -E '^ui05v_play_state_regression$' --output-on-failure
```

O binário OpenGL completo não foi vinculado aqui: falta `GL/gl.h` no ambiente.
Ele deve ser compilado e testado na máquina Linux que possui as dependências
OpenGL e os assets históricos Client.

## Próximo trabalho

Para serviços além de forja, combinação e encaixe (reparo, extração de gemas,
apparel), comparar as chamadas concretas de `Client/scripts/lua/forms/NPC.clu`
e os scripts `Client/scripts/offline/...` **antes** de implementar regras ou
abrir abas que não existem. Esse arquivo histórico não foi incluído no ZIP de
sources; nenhum comportamento foi inferido sem ele.
