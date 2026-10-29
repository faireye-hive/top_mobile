# Tales of Pirates — migração 05-AD

Base: **05-AC**. Evolução: **diálogo de NPC -> bancada visual de forja**.

## Objetivo

Abrir a bancada de forja a partir de um NPC que declara `OpenForge`, `OpenUnite` ou `OpenMilling` em seu script Lua histórico. A execução permanece offline; o código **lê os menus Lua como dados**, sem interpretar ou executar código do GameServer.

## Recursos incluídos

- `NpcSystem::forgeServices(script)` percorre os arquivos históricos `Client/scripts/offline/script/MisScript/NpcScript01.lua` a `NpcScript08.lua` e identifica as chamadas `Text(..., OpenForge)`, `Text(..., OpenUnite)` e `Text(..., OpenMilling)` **dentro da função Lua associada ao NPC**. Comentários e chamadas de outras funções são ignorados.
- Goldie (`r_trade01` no `NpcScript01.lua`) declara os três serviços e apresenta o botão **Forge**, mantendo **Trade** e as missões já existentes.
- O botão Forge encerra o diálogo e abre o painel `ForgeWorkbenchUI` com a bolsa posicionada ao lado.
- As abas da bancada ficam limitadas aos serviços realmente declarados pelo NPC. Abas indisponíveis são exibidas em cinza e não podem ser selecionadas.
- Confirmação, custos, chances, materiais e validação de UIDs continuam nas regras `Play` estabelecidas em 05-Z/05-AA/05-AC.
- Somente é possível abrir a bancada a partir do diálogo quando o jogador ainda está a **até 2,4 unidades** do NPC e no mesmo mapa onde a conversa começou.
- `Alt+B` permanece como atalho de desenvolvimento offline, com todas as abas disponíveis.
- Saves 05-Y/05-Z/05-AA/05-AC são preservados; nenhum campo de save foi alterado.

## Como usar

1. Inicie o jogo em `garner` (Argent City), visite **Blacksmith - Goldie** nas coordenadas aproximadas **2193, 2706**.
2. Clique na NPC e aproxime-se. Na conversa, selecione **Forge** (quando houver missão ativa, o botão aparece logo acima da linha de respostas).
3. Na bancada, use as abas `Forjar`, `Combinar`, `Encaixe`, dependendo dos serviços do NPC. Clique nos itens da bolsa ou arraste para os três espaços.
4. Revise o custo e clique em **Revisar e confirmar**; a operação só é executada depois do **Confirmar**.

**Nota:** esta migração não recria a interface exata da versão DirectX original, nem implementa reparos de equipamentos, extração de gemas, fusão de apparel ou interpretador Lua completo. Serviços ausentes do script não são concedidos apenas porque o NPC se chama Blacksmith. Caso faltem os `NpcScript*.lua` do cliente, o botão Forge não será exibido (o atalho `Alt+B` continuará funcional).

## Compilação no Linux

```bash
cmake -S sources -B build
cmake --build build -j4
./build/top_gl --check
./build/top_gl --profile
```

Banner: `top_gl offline - migracao 05-AD (ferreiro NPC e servicos Lua na bancada visual)`.

## Regressão

```bash
cmake -S sources -B build-ui-tests -DTOP_BUILD_UI_TESTS=ON
cmake --build build-ui-tests -j4
ctest --test-dir build-ui-tests --output-on-failure
```

**Resultado neste ambiente: 44/44 testes aprovados.** O teste adicional `ui05ad_historical_npc_forge_service_binding` cobre leitura do script, seleção de serviços e restrição das abas. `Play::selfTest()` também verifica `Trade` e `Forge` no mesmo diálogo. Os testes são rápidos, mas compilam módulos C++ extensos na primeira execução.

**Limitação do ambiente de compilação:** não existem cabeçalhos de desenvolvimento OpenGL (`GL/gl.h`), portanto a compilação completa `top_gl` não pôde ser executada aqui; as unidades de interface, NPC e jogo atualizadas foram compiladas por seus testes e as verificações de sintaxe C++17 de `main.cpp`, `Play.cpp` e `ActorSkinUIBridge.cpp` passaram. A compilação OpenGL deve ser validada no Linux com as bibliotecas de desenvolvimento instaladas.

Se o `gmake` avisar `Clock skew detected`, a causa costuma ser a data futura dos arquivos extraídos. Em caso de necessidade, execute `find sources -type f -exec touch {} +` e recompilie.

## Próximos passos

Estudar a interface de ferreiro original de `Client/scripts/lua/forms/NPC.clu` e sistemas adicionais (reparo, extração, apparel) antes de migrá-los. O estado da bancada permanece em C++, desacoplado das regras de consumo.
