# Tales of Pirates — Migração 05-AM

## Escopo

Continuação da 05-AL (cliente Linux/OpenGL offline): **elegibilidade e dependências das missões históricas**. Os scripts Lua são lidos passivamente, **não executados**. Não há alteração no save, nos modelos, no renderizador, nas regras de itens nem no número de missões simultâneas (8).

## Problema observado

Na 05-AL o diário já tinha missões simultâneas e a UI mostrava ofertas dos NPCs, mas só aplicava nível e repetição. As condições Lua `MisBeginCondition(...)` de centenas de missões não eram interpretadas, então o jogo tratava várias ofertas como disponíveis mesmo sem concluir a missão anterior, possuir a profissão correta ou cumprir outras condições.

## Mudanças

- **Novo `MissionEligibility.h`**: representação passiva de dependências e restrições de início.
- **`NpcSystem.cpp`**: identifica `HasRecord`, `NoRecord`, `HasMission`, `NoMission`, `PfEqual`, `NoPfEqual`, `HasItem` e comparações de nível `LvCheck` (`>`, `>=`, `<`, `<=`) nas linhas de `MisBeginCondition`. Mantém a exclusão de `AlwaysFailure`.
- **ID histórico de registro**: `DefineMission(displayId, title, missionId)` nem sempre coincide com `MisBeginAction(AddMission, recordId)`. O leitor cria um índice `recordId → missionId` a partir do Lua para que condições `HasRecord` e semelhantes possam consultar as missões do save, inclusive após reabertura.
- **Fail closed**: condições ainda não implementadas (como `CheckConvertProfession`, guildas, `CheckBag` e flags) bloqueiam a aceitação. Nenhuma profissão, requisito ou progresso é inventado.
- **`Play.cpp`**: usa um único verificador de elegibilidade na UI, nos marcadores `!` dos NPCs e na aceitação por menu. Revalida no instante do clique, para impedir uma missão de ser aceita depois de perder requisitos. Pré-requisitos são verificados usando missões ativas/concluídas persistidas pela 05-AL.
- **Interface procedural**: missões bloqueadas continuam visíveis para inspeção, com status *Indisponível* e motivo no painel de detalhes (ex.: `Conclua primeiro a missao 900`, `Exige nivel 20`, `Exige profissao 4`, `Condicao Lua: CheckBag (indisponivel offline)`). Não é possível aceitá-las. Pode-se implementar opção de ocultá-las em migração posterior.
- **`main.cpp`**: propaga as condições Lua e o índice de registros aos marcadores dos NPCs.

## Limites / não afirmar fidelidade total

- O parsing das condições de missão é **passivo e baseado nas chamadas por linha** dos `MissionScript*.lua`. Não executa expressões ou funções Lua. Regras dinâmicas e auxiliares do GameServer não foram reproduzidas.
- Um registro (`HasRecord`) concedido por `SetRecord` sem correspondência a uma missão concluída ainda não está modelado separadamente. Nesses casos, a missão fica bloqueada; **não** forçamos uma liberação fictícia.
- A entrega de missões em NPCs continua pendente: a lógica offline anterior pode completar objetivos automaticamente. Isso não foi alterado.
- A lista do NPC **ainda mostra missões bloqueadas com seus motivos**. Se a meta for copiar exatamente a visibilidade original, a próxima etapa pode alternar `Disponíveis`/`Todas` sem alterar as validações.
- Saves `Client/user/`: formato não modificado; fazer backup antes de substituir os fontes.

## Amostra do cliente histórico real

Com `Client/scripts/offline/script/MisScript/` e `garner` (Argent City): 153 NPCs e 650 IDs vinculados, dos quais 319 receberam dados válidos do parser. Entre essas missões válidas, 225 possuem pelo menos um requisito `HasRecord`, 88 possuem limite de nível, 5 condição simples de profissão e 20 alguma condição especial não suportada. Os grupos se sobrepõem; **não** devem ser somados para gerar uma taxa de disponibilidade.

## Testes e compilação

- **56/56 testes headless aprovados** (`ctest`), com os scripts históricos acessíveis em `Client/`.
- Novo teste `ui05am_mission_prerequisites_and_job`: fixture artificial com índice `AddMission` diferente do ID de apresentação, `HasRecord`, `NoRecord`, `NoMission`, `LvCheck`, `PfEqual`, `NoPfEqual`, `HasItem`, `AlwaysFailure` e condição não suportada.
- `Play::selfTest` estendido: missão 2 bloqueada por missão 1, desbloqueio após concluir, exclusão por profissão/regra não suportada e persistência após save/reload.
- `NpcSystem.cpp`, `Play.cpp` e `main.cpp` passaram na verificação `g++ -std=c++17 -fsyntax-only`.
- **O binário OpenGL integral não foi recompilado neste ambiente** (headers de desenvolvimento GL ausentes), precisa compilar no Ubuntu do usuário.

## Comandos no Ubuntu

```bash
cmake -S sources -B build -DTOP_BUILD_UI_TESTS=ON
cmake --build build -j4
ctest --test-dir build --output-on-failure
./build/top_gl --check
./build/top_gl
```

## Próximo passo sugerido: 05-AN

Fazer a etapa de **entrega de missões ao NPC (MisResultCondition/MisResultAction)**, sem conceder recompensa prematuramente. Priorizar fluxo Aceitar → Progredir → Entregar ao NPC correto → Receber recompensa, avaliar flags e registros `SetRecord` apenas onde for possível justificar pelo código histórico; acrescentar testes de save. Preservar toda a 05-AM e continuar com `PROMPT_PROXIMA_MIGRACAO.md` atualizado.
