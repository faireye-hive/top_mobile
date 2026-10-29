# Tales of Pirates OFFLINE — Migração 05-AQ

Data: 22/10/2026. Base **exata**: `TalesOfPirates_05-AP_Completo.zip`.

## Escopo implementado (primeira etapa das flags históricas)

- Em `NpcSystem`, leitura **passiva** de `MisBeginAction(SetFlag, recordId, flag)` e das condições `MisResultCondition(HasFlag/NoFlag, recordId, flag)`, além de `MisResultAction(SetFlag/ClearFlag, recordId, flag)` nos caminhos **com `ClearMission(recordId)`**. O `recordId` usado é o de `DefineMission(..., recordId)`/`AddMission`, não o ID de exibição. Não executa Lua.
- A entrega com `HasFlag/NoFlag` só é liberada quando o valor pode ser demonstrado pela flag inicial: `HasFlag` de uma flag inicial e `NoFlag` de uma flag ausente. Se a missão usa `AddTrigger` e possui condições de resultado por flag, **permanece bloqueada**, pois não seria correto inventar o estado produzido pelo GameServer. Flags de outros registros e parâmetros desconhecidos continuam bloqueados. A oferta não é liberada somente por proximidade do NPC.
- `Play` copia flags iniciais e condições de resultado para o progresso **individual** de cada missão aceita via NPC. A prontidão/entrega verifica `HasFlag` e `NoFlag` além dos objetivos de item/kill. Os demais mecanismos de 05-AP (NPC correto, inventário, prêmio único e várias missões) foram mantidos.
- `|Q4:...` é um **campo opcional final** para flags de missões ativas com entrega, após `Q3`, sem reordenar ou substituir quaisquer campos anteriores (`Q2`, `QH2`, `Q3`). Formato: `Q4:missionId~flagsAtivas~flagsNecessarias~flagsProibidas;...`, com números de flags separados por ponto e `-` para vetor vazio. Entradas não conformes são descartadas defensivamente. Saves anteriores sem `Q4` continuam legíveis.
- Ajustada a mensagem do executável `top_gl` para mostrar a migração 05-AQ, em vez do cabeçalho antigo 05-AO.
- UI procedural, renderização OpenGL, espadas e animações, inventário UID, itens, forja, mapas e portais não foram alterados.

## Evidência nos fontes originais

- `fontes/Server/GameServer/src/Mission.cpp`: `MisSetFlag(wRoleID,wFlag)`, `MisClearFlag` e `MisIsSet` operam na `RoleInfo` da missão ativa identificada por `wRoleID` (registro da missão).
- `MissionScript01.lua`: `Leaves Collection` registro **721** inicia com `SetFlag(721,1)` e `Decoction Recipe` registro **726** inicia com `SetFlag(726,1)`; ambas continuam entregáveis segundo a 05-AP, pois suas condições de entrega não exigem flags dinâmicas.
- `Blacksmith's Greetings` registro **701** tem `SetFlag(701,1)` ao iniciar, mas a entrega exige `HasFlag(701,10)` que depende de evento não reproduzido; o bloqueio conservador foi mantido.
- Script Lua de teste **sintético e isolado** confirma que uma entrega com `HasFlag(record,1)` + `NoFlag(record,10)` é elegível quando `SetFlag(record,1)` está na abertura e não há gatilho externo. Um caso com `AddTrigger` continua bloqueado. Não confundir esses testes de parser com uma missão histórica comprovadamente disponível.

## Validação executada

- Verificação de sintaxe: `g++ -std=c++17 -fsyntax-only` para `NpcSystem.cpp`, `Play.cpp`, `main.cpp` e `UI05AQTests.cpp`: **aprovada**.
- Compilação incremental dos alvos headless pelo CMake: **aprovada**.
- `ctest --test-dir .../build-headless --output-on-failure -j4`: **59/59 testes aprovados**, incluindo `ui05aq_mission_scoped_flags` e a regressão `ui05v_play_state_regression` (o `Play::selfTest()` exercita gravação/carregamento de Q4, condição proibida e entrega única).
- Não há SDK OpenGL completo nesta máquina; `top_gl` **não foi compilado nem executado graficamente aqui na 05-AQ**. O usuário confirmou anteriormente em seu Ubuntu o carregamento e o primeiro quadro da **05-AP** (58/58), o que não substitui a validação da 05-AQ.

## Limitações — importante para 05-AR

- Ainda não existem emulação segura dos eventos `TE_GETITEM`, `TE_KILL`, `AddNextFlag` e `AddTrigger`; não habilitar missões com flags dinâmicas presumindo que a flag veio de uma conversa.
- `MisBeginAction(GiveItem)` (ex.: entrega de carta) e destinatário diferente do ofertante não foram cobertos nesta versão.
- Não há suporte para sequências completas de múltiplas fases/flags ou todos os objetivos de diferentes monstros; `AddExp(a,b)` segue premiando o limite inferior legado da 05-AO.
- `SetFlag`/`ClearFlag` em resultado de missão terminada não fornece um armazenamento de flags global: a instância é encerrada, conforme fluxo `ClearMission`. Sem afirmar que esta etapa reproduz todo o GameServer.
- Campo Q4 sem dados (saves 05-AP antigos) conserva as regras do próprio save, sem inferência retroativa.

## Testar no Ubuntu

Fazer backup de `Client/user/`. Substituir somente `sources/` pelo ZIP 05-AQ; **não sobrescrever `Client/` ou saves**. Evitar timestamps futuros no diretório extraído (verificar relógio e usar `find sources -type f -exec touch {} +` se necessário). Recomenda-se limpar a pasta `build` ao migrar de versão:

```bash
rm -rf build
cmake -S sources -B build -DTOP_BUILD_UI_TESTS=ON
cmake --build build -j4
ctest --test-dir build --output-on-failure
./build/top_gl --check
./build/top_gl
```

Validar especialmente que o cabeçalho exibe 05-AQ, que `Welcome`/Senna e entregas de item 05-AP continuam funcionando, que missões dependentes de gatilho ainda não oferecidas não aparecem indevidamente, e que o save com `Q4` é lido após reiniciar. Não modificar manualmente `Client/user/` sem backup.
