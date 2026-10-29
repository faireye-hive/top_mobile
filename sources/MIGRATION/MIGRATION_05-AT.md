# Tales of Pirates OFFLINE — Migração 05-AT

Base **05-AS**. Escopo solicitado: refatoração conservadora para separar **UI**, **NPCs**, **missões** e **renderização**. Esta migração é arquitetural; NÃO implementa novos gatilhos históricos nem modifica recompensas ou formato de saves.

## Organização modular

```text
sources/
  Engine/include/top/
    MissionTypes.h        # HistoricalMission e HistoricalMissionStep
    MissionCatalog.h      # interface do parser/catálogo de missões Lua
    MissionEligibility.h  # requisitos históricos
    SavePaths.h           # caminho de save por personagem
    ProgressionPoints.h   # pontos de atributo/skill
    PresentationTypes.h   # DTOs para apresentação (sem ActorSkin)
    VertexTypes.h         # tipos de vértices (sem OpenGL)
    NpcSystem.h           # contratos NPC + adaptadores antigos
    Play.h                # fachada legada de gameplay
  Engine/src/
    npc/
      NpcSystem.cpp          # npclist, NPCs próximos, diálogos e lojas
      PlayNpcInteraction.cpp # ações de diálogo encaminhadas ao gameplay
    mission/
      MissionCatalog.cpp     # leitura passiva de MissionScript*.lua
      PlayMissions.cpp       # requisitos, progresso, flags, aceite, abandono, recompensa
    top/
      Play.cpp               # núcleo restante: combate, personagem, mapa, inventário
    gl/                      # backend OpenGL/GLX inalterado
  Engine/ui/
    QuestDialogInput.h       # hit-test procedural do diálogo
    screens/                # apresentação de NPC, Quest, inventário etc.
    ActorSkinUIBridge.cpp    # adaptador explícito UI → gráficos 3D
```

### Alterações na camada de missões

- `HistoricalMission` e `HistoricalMissionStep` saíram de `NpcSystem.h` e agora pertencem a `MissionTypes.h`, sem dependência de NPC, X11 ou OpenGL.
- O parser passivo (`DefineMission`, `MisBegin*`, `MisResult*`, `COMPLETE_SHOW`, IDs de exibição/registro), seus índices e caches foram **movidos** de `NpcSystem` para `MissionCatalog`.
- `NpcSystem::mission`, `missionByDisplay`, `missionStep` e `missionRecordIndex` continuam como **fachadas de compatibilidade**, delegando exclusivamente a `MissionCatalog`. Isso preserva consumidores/testes legados e permite nova migração progressiva para `NpcSystem::missions()`.
- As operações de missão do personagem em `Play.cpp` foram movidas para `PlayMissions.cpp`: `missionLockReason`, `applyNpcMissionStep`, `missionReadyAtNpc`, `deliverWelcomeToSenna`, `deliverHistoricalToNpc`, `setNpcMissionList`, `offerHistoricalMission`, `acceptHistoricalMission`, consulta/seleção, abandono, objetivos, capacidade de inventário e conclusão/recompensa.
- `PlayNpcInteraction.cpp` cuida de abrir o diálogo com NPC, selecionar entradas e despachar ações da UI. **Não implementa diretamente regras de experiência/recompensa**.
- `SavePaths.h` e `ProgressionPoints.h` compartilham funções antes internas a `Play.cpp`, evitando cópias divergentes após a separação.

### Alterações na UI

- A interface virou um alvo CMake próprio: `top_ui` (**biblioteca estática**); `top_gl` apenas a integra.
- `UIRenderer` e `HudComposer` consomem `PresentationTypes.h`, sem incluir `ActorSkin.h`.
- `VertexTypes.h` extrai o tipo `MeshVertex` de `GfxDevice.h`, sem dependência do OpenGL.
- `QuestDialogInput.h` faz exclusivamente hit-test com dados de estado; `Play::dialogActionAt()` não contém mais geometria dos botões. `clickDialog()` continua a **revalidar** aceite e entrega no gameplay.
- `ActorSkinUIBridge.cpp` permanece, intencionalmente, como adaptador entre lotes de UI e desenho do personagem, isolado fora da biblioteca `top_ui`.

### Preservado

- **Toda a jogabilidade da 05-AS**: Welcome e Senna; Senna → Goldie → Senna, carta 3950, flag 701/10 e 9 EXP; NPC com `!` verde e `?` amarelo; botões Aceitar/Completar; múltiplas missões, requisitos e catch-up de níveis anteriores.
- Nenhuma alteração nos campos de save `Q2`, `QH2`, `Q3`, `Q4`, formatos de arquivos históricos ou pasta `Client/`.
- Nenhuma alteração em posicionamento das armas, renderização de terreno/mapas, habilidades, forja, inventário, combate e networking (segue OFFLINE).

### Validação realizada

- Build `top_ui`: **OK** (sem GL runtime).
- Build do alvo `top_regression_tests`: **OK**.
- `ctest` com `Client/` de fixtures históricas: **63/63 testes aprovados**, incluindo todos os testes anteriores e os novos `ui05at_decoupled_quest_dialog` e `ui05at_independent_mission_catalog`.
- `g++ -std=c++17 -fsyntax-only`: **OK** em `Play.cpp`, `PlayMissions.cpp`, `MissionCatalog.cpp`, `NpcSystem.cpp`, `PlayNpcInteraction.cpp`, `ActorSkinUIBridge.cpp`, `Platform/linux/main.cpp`.
- Patch incremental aplicado sobre o ZIP completo 05-AS e comparado byte a byte com o ZIP atualizado: **verificação executada no empacotamento**.
- O SDK OpenGL completo (`GL/gl.h` e `libGL.so`) não está disponível neste ambiente; **NÃO** foi validada a compilação/execução `top_gl` da 05-AT. Isso é responsabilidade do teste no Ubuntu.

## Limitações e próxima etapa

A refatoração separa arquivos, tipos, compilações e componentes, mas **Play continua uma fachada** usada por diferentes domínios; a transferência completa do estado de missão para uma classe `MissionService` independente e a eliminação definitiva dos métodos legados em `NpcSystem` ficam para uma migração posterior com testes de save/reload. Não declarar que todas as dependências estão removidas.

A próxima migração, **05-AU**, pode implementar os gatilhos históricos `TE_GETITEM`, `TE_KILL` e `AddNextFlag`, com a lógica diretamente em `Engine/src/mission`, não em `NpcSystem` nem no renderizador. Priorizar regras comprovadas pelos Lua/GameServer antigo e falhar fechado em casos desconhecidos.

## Validar no Ubuntu

**Faça backup de `Client/user/`.** Substitua somente `sources/`; não sobrescreva `Client/`.

```bash
cmake -S sources -B build -DTOP_BUILD_UI_TESTS=ON
cmake --build build -j4
ctest --test-dir build --output-on-failure
./build/top_gl --check
./build/top_gl
```

Confirmar mensagem `05-AT`, mapas/atores/armas, abrir janela NPC, ofertar missões anteriores elegíveis, concluir Welcome com botão Completar, realizar Senna → Goldie → Senna, testar recompensa única e salvamento após reinício. Se o CMake reutilizar objetos antigos, reconstruir só `build/`; não apagar `Client/user/`.
