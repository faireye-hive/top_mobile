# Estado do porte Linux/OpenGL

Atualizado em 23/10/2026. Documento principal de diagnóstico e planejamento. Substitui os levantamentos e roteiros antigos removidos nesta revisão. Os arquivos MIGRATION_*.md descrevem etapas históricas, não certificam o estado atual.

## Atualização de plataforma — 29/10/2026

Adicionado alvo web Emscripten/WASM + WebGL 2, compartilhando o gameplay C++ com
o Linux. O loop agora fica em `Platform/common/GameMain.cpp`; as entradas e
janelas são adaptadores por plataforma. Build Linux e build web Release
aprovadas, com Garner renderizado nas duas e salvamento/restauração local
verificados no navegador. Recursos são baixados sob demanda; saves Linux não
são incluídos no pacote público.

Uso: [guia web](../offline/web/README.md). Evidências e limites:
[validação de 29/10](../deliverables/web-2026-10-29/VALIDACAO.md).
A suíte nativa atual ficou em 142/162, com 20 falhas; duas foram reproduzidas
numa cópia sem as mudanças desta tarefa. Não interpretar os 71/71 históricos
abaixo como resultado atual. A estimativa de fidelidade de 23/10 não foi
recalculada por esta entrega de plataforma.

## Escopo e conclusão

Comparação entre o executável C++17 offline compilado por sources/CMakeLists.txt, as fontes originais de cliente/engine em arquivo/fontes, os scripts históricos do GameServer e os recursos de Client/. O alvo considerado é a experiência de jogo offline, com aparência e regras equivalentes onde aplicáveis.

**Estimativa de planejamento: aproximadamente 55% do porte offline, com faixa de incerteza de 45–65%.** Há uma base jogável e uma infraestrutura significativa; ainda falta bastante fidelidade, cobertura de regras/conteúdo e validação em sessão real. A faixa não é estatística e não representa porcentagem de linhas de código, horas restantes nem aprovação de 55% de uma lista completa de comportamentos.

A rede do MMORPG, autenticação, grupos de jogadores, guildas online, comércio entre jogadores e infraestrutura de servidores ficam fora desse denominador. Incluí-los exigiria outro inventário e outro orçamento. Sistemas individuais como progressão, produção, pets e navegação precisam ser classificados dentro do escopo offline; ter sua janela ou seus itens no catálogo não comprova a mecânica.

## Como cheguei à estimativa

Pesos e notas abaixo são julgamento técnico explícito para priorização, baseados nos caminhos de implementação inspecionados. Não são cobertura medida por testes. A soma ponderada é 54,45 pontos de 100, arredondada para 55. Conteúdo não auditado integralmente reduz a confiança.

| Área | Peso no escopo | Nota estimada de conclusão | Evidência e principal lacuna |
|---|---:|---:|---|
| Plataforma, build e carga de recursos | 8 | 80% | Runtime Linux/OpenGL e leitores funcionais; distribuição e portabilidade ainda incompletas. |
| Mapas, colisão, população e deslocamento | 12 | 65% | Mapas, portais, pathfinding, spawns e memória de monstros; faltam recursos e equivalência da dinâmica/eventos. |
| Renderização do mundo, materiais e mar | 12 | 50% | Terreno, objetos, luz e água têm implementação; mar em seções vazias diverge e falta comparação visual sistemática. |
| Personagens, animações e efeitos | 15 | 40% | Skinning, poses e tempos existem; canais, pontos de ligação e eventos de efeitos estão incompletos. |
| Combate, atributos, skills e estados | 15 | 55% | Diversas fórmulas históricas portadas; permanecem aproximações e callbacks genéricos. |
| Itens, equipamento, economia e produção | 10 | 65% | Bolsa, UIDs, lojas, cofre, forja e parte da geração; uso de itens e sistemas avançados têm cobertura parcial. |
| Missões, diálogos e progressão de classes | 10 | 50% | Aceitar/progredir/entregar e condições suportadas; não há execução geral da semântica Lua. |
| Barcos e jogo naval | 8 | 25% | Modelos, movimento e canhão local; velocidade/dano e fluxo de embarque ainda simplificados. |
| Interface, seleção de personagens e áudio | 5 | 65% | Várias telas, perfis e áudio por área; faltam fluxos históricos, personalização e validação visual. |
| Persistência, qualidade e distribuição | 5 | 65% | Save com checksum/backup e testes; falta matriz de equivalência, pacote reproduzível e validação ampla. |

A infraestrutura está mais avançada que a fidelidade. Os 52 mapas válidos não significam 52 mapas integralmente portados; os 411 registros visuais de skills não significam 411 skills corretas.

## Evidência executada nesta revisão

- Build completo de build/: aprovado.
- CTest: **71/71 aprovados**.
- ./build/top_gl --check: aprovado no cenário inicial de Garner.
- Auditoria de recursos reexecutada: 53 entradas de mapas, 52 cabeçalhos válidos; 47 arquivos .obj examinados.
- 266.852 posicionamentos de objetos; 628 modelos distintos utilizados; **78 modelos utilizados ausentes**, afetando **1.944 posicionamentos em oito mapas**; 32 posicionamentos com ID não resolvido.
- Uma falta obrigatória, 87 opcionais e 13 diferenças de maiúsculas/minúsculas no inventário geral.
- O check carrega os 30 quadros de água e 411 registros de visuais de skills.
- Não houve sessão gráfica comparativa com o cliente original nesta análise. Os testes acima não certificam qualidade visual, controles em uso prolongado ou funcionamento integral de todos os mapas.
- A build examinada está sem CMAKE_BUILD_TYPE definido. Há testes que usam assert; builds Release com NDEBUG podem eliminar verificações. Isso precisa ser corrigido antes de usar resultados Release como garantia.

Resultados desta revisão estão em auditoria-2026-10-23/ com prefixo revisao-atual-. A auditoria de recursos comprova arquivos/cabeçalhos e referências, não execução completa de scripts.

## Achados prioritários, comparados com o original

### 1. Mar: implementação existente, mas tratamento de regiões vazias divergente — prioridade imediata

Original: arquivo/fontes/Engine/include/MPMap.h, GetTile, retorna um tile padrão quando a seção não tem dados. MPMap.cpp configura esse tile com UNDERWATER_HEIGHT=-2.0 e textura 22. RenderSea usa esse fundo e as quadras de 4 m.

Atual: Engine/src/top/MapFile.cpp retorna Tile inválido em seção sem dados; TerrainMesh.cpp ignora tiles inválidos antes de ativar seaNeeded e dá alpha zero aos vértices de água cujo tile é inválido. Existe geração de água em mode 7, shader e seleção das 30 texturas em Platform/linux/main.cpp.

**Consequência demonstrável pelo código:** uma janela só com seções sem dados não ativa a geração de mar; em janelas mistas, amostras inválidas ficam transparentes. É uma causa provável do mar ausente relatado, ainda sem confirmação gráfica do local específico. sailableAt também recusa tiles inválidos; desenho e navegação precisam de contratos coerentes.

Próxima tarefa: reproduzir a política histórica de fundo submerso dentro dos limites do mapa, distinguir seção legitimamente vazia de arquivo ilegível e revisar navegação sem liberar área corrompida/fora do mapa. Não inventar água moderna/reflexos como requisito: primeiro igualar o caminho DX9 realmente usado.

Aceite: costa, mar aberto sem seção, borda de mapa e chegada de barco; captura com mesma câmera/posição no original; teste de malha/alpha e regressão de colisão.

### 2. Skills: temporização melhorou, composição visual ainda parcial — prioridade imediata

Original: Client/src/STAttack.cpp dispara efeitos do personagem, item, voo e alvo em keyframes próprios, com pontos de ligação (dummies). Libraries/common/src/SkillRecord.cpp lê listas de efeitos, tipos, ligação à arma, modos de seleção de ação e quadros de disparo.

Atual:
- SkillVisual.h guarda apenas o primeiro ID de cada campo; castId escolhe actor OU item OU itemExtra; hitId escolhe target OU ground OU water. Isso não representa todos os canais nem a seleção correta pela superfície.
- EffectSystem::emit cria uma instância selecionada por evento. Há suporte real a .eff, partículas, malhas nativas e .lgo, mas também fallbacks procedurais.
- Malhas de efeitos usam, em caminhos inspecionados, posição no chão + 0,9 m. Isso não equivale à matriz de mão/arma/corpo do original.
- ActorSkin::PoseSpan retém um impacto por ação, enquanto o original possui contagem/lista de keyframes.
- choosePoseAction percorre a lista por sequência; o carregador não lê o modo histórico chActionPlayType da coluna 46.
- As correções recentes sincronizam duração, velocidade, início/impacto/voo e cancelamento de eventos. Elas não resolvem todos esses pontos de composição e seleção.

Próxima tarefa: selecionar algumas skills representativas e concluir o caminho inteiro: ação, todos os eventos, dummies, voo, alvo/solo/água, mistura de material e encerramento. Investigar transições/cancelamentos junto com a máquina de estados original. Não resolver adicionando partículas genéricas a toda skill.

Aceite: ataque corpo a corpo, arco/arma de fogo, Illusion Slash, magia com voo, buff e skill de área; quatro raças, armas compatíveis, 30/60 FPS, repetição, interrupção e alvo morto. Comparar vídeo em velocidade normal e quadro a quadro.

### 3. Combate e estados: regras históricas convivem com aproximações — criticidade alta

Há classificações explícitas para várias fórmulas, além de implementações recentes de Heal, Berserk, Harden, Will of Steel, Shadow Slash, Poison Dart, Enfeeble e callbacks de monstros. Os atributos e bônus de equipamento também avançaram.

Entretanto, PlayCombat.cpp ainda infere certos buffs/curas a partir de palavras como heal, recover, shield e protect; SkillRule começa em DamageFormula::Generic; algumas fórmulas ausentes usam alternativas locais. Isso pode permitir uma skill aparentemente funcional com números e efeitos diferentes.

Faltam cobertura e validação sistemáticas de passivas, resistências, estados, remoção/reaplicação/acúmulo, alvos e formatos de área, exceções de mapas/bosses, bônus especiais e interações entre equipamentos e skills. As classes avançadas precisam de seu fluxo completo de promoção e conteúdo; ler coeficientes de classe não o implementa.

Próxima tarefa: catálogo explícito de suporte por callback/ID, com origem histórica e casos de teste independentes. Diferenciar fiel, parcial, aproximado e indisponível na ferramenta de diagnóstico. Priorizar a classe que será jogada do nível inicial até a promoção seguinte.

### 4. Itens e economia: já há bastante coisa; geração histórica não está completa — criticidade alta

Implementados: inventário, pilhas, identidades de instância, equipamento/requisitos, lojas, cofre, várias caixas/tickets, gemas, sockets e operações de forja. Não devem voltar à lista de “não portados”.

O código atual de ItemInstantiationRules.cpp já possui geração de qualidade/afixos para os eventos 11 e 95 e tipos delimitados, usando UID como semente local. PlayItems.cpp prepara recompensas antes de consumir a caixa e rejeita geração não suportada. Portanto, os relatórios antigos que descreviam somente defaults e todas essas caixas bloqueadas foram superados.

Falta generalizar as origens/tipos realmente presentes em Creat_Item, verificar distribuição e atributos contra o Lua, ampliar callbacks de uso e auditar produção avançada, pets/fadas e bônus vinculados. Semente determinística local é uma decisão offline; não demonstra equivalência do RNG do servidor.

Aceite: comprar/ganhar/usar/equipar/salvar/reabrir sem perder UID ou atributos, bolsa cheia sem consumo parcial e probabilidades verificadas para cada família suportada.

### 5. Missões e NPCs: subconjunto funcional, não um motor completo de scripts — criticidade alta

MissionCatalog.cpp reconhece um conjunto de condições/ações e registra unsupported para outras. Existem eventos de abate/coleta, histórico, diário, requisitos e entrega. PlayMissions.cpp bloqueia condições não suportadas. A primeira promoção possui regra histórica de nível 9/2.500 ouro/restrições de raça.

NPCs ainda usam extração de scripts. Reconhecer a presença de OpenBank não reproduz arbitrariamente toda a árvore condicional do diálogo. Recompensas e eventos complexos precisam de inventário próprio; AddExp com intervalo ainda tem caminho de limite inferior determinístico.

Falta: cadeias multi-NPC, triggers adicionais, flags/ramificações, promoções avançadas e missões de sistemas especiais. Decidir explicitamente quais eventos temporários pertencem à versão offline. Não ativar todo CreateCha extraído como se todos os scripts de evento devessem rodar juntos.

### 6. Barcos: uma das maiores lacunas de funcionalidade — dificuldade alta

Atual: cascos, altura de navegação, modo barco, caminho em água, esteira, seleção e canhão. Contudo, main.cpp usa velocidade de barco fixa em 10; embarque/desembarque tem atalho com procura de terreno próximo; fireBoatCannon usa dano 18 + nível*3 - defesa/3 e recarga fixa de 1,8 s.

Isso é uma base de teste, não o sistema naval completo de CharBoat.cpp e dos scripts/tabelas históricos. A fórmula portada para o callback de canhão de monstros não substitui esse disparo local do jogador.

Faltam verificar/implementar: identidade persistente da embarcação, construção/compra, partes e atributos, HP/recursos, reparo/abastecimento, carga, progressão naval, regras de portos e habilidades. Separar o estado do barco do estado do personagem antes de ampliar a economia naval.

### 7. Mundo, materiais, IA e conteúdo — dificuldade variável

A navegação usa regiões LAND/BRIDGE e grade de bloqueio; há portais, spawns e memória de HP/morte/respawn por região. A IA possui perseguição, fuga, retorno e linha de visão. São avanços reais.

Falta auditar a população e a lógica por mapa, eventos/bosses, transições longas e limites de entidades locais. Recursos ausentes exigem buscar correspondência no acervo antes de considerar modelagem ou substituição. O contador de modelos animados no check não prova que toda animação/material foi reproduzido.

Renderização: conferir transparência e ordem de desenho, misturas de material, luz, sombras, animação UV/texturas e efeitos anexados. Não assumir que neblina, reflexos ou outros recursos são obrigatórios apenas por existirem em algum trecho antigo: confirmar o caminho habilitado na versão alvo.

### 8. Interface, áudio, saves e entrega — dificuldade baixa a alta conforme o contrato

UI foi separada em top_ui. Há inventário, habilidades, missões, loja, cofre, forja, status, minimapa, barcos, sistema e seleção de personagens. Perfis têm identidade independente da raça; não se deve descrever a seleção como totalmente ausente.

Pendências: personalização completa, equivalência dos fluxos/telas, texto extenso, escalas/resoluções, foco e arraste, serviços antigos ainda sem mecânica correspondente. Música por área já existe; conferir gatilhos, volumes, sons de skills/impactos e sons ambientes.

SaveRecovery.cpp está integrado ao Play: checksum, backup, recuperação e gravação atômica já existem. Falta auditar o conjunto de estado que deve sobreviver a sair/reabrir, posição/mapa, efeitos, entidades e futuras extensões navais. Não confundir preservar monstros durante streaming com persistir todo o mundo entre execuções.

CMake instala apenas o executável. Faltam pacote de recursos/dependências, localização estável dos dados, instruções de distribuição, build limpa e testes em outra instalação. A árvore de trabalho tem muitas mudanças/arquivos ainda não versionados; uma referência reproduzível deve ser estabelecida antes de grandes frentes simultâneas, preservando todo o trabalho local.

## Sequência recomendada e modelo por tarefa

As escolhas abaixo são recomendações para este repositório, não resultado de um benchmark local entre modelos. A documentação atual recomenda GPT-6.1 Sol para trabalho complexo recorrente, Astra para os problemas mais exigentes e Luna para tarefas claras/repetíveis. Fonte: [modelos e esforço de raciocínio](https://learn.chatgpt.com/docs/models), consultada em 23/10/2026.

| Ordem | Entrega concreta | Dificuldade | Modelo / esforço sugerido | Critério de conclusão |
|---|---|---|---|---|
| 1 | Corrigir mar/seções vazias e verificar costa | Média; sobe se envolver render state | GPT-6.1 Sol High; Astra High se a divergência persistir | Mar aberto e costa comparados, navegação e limites testados. |
| 2 | Uma família de skills fiel de ponta a ponta | Muito alta | GPT-6 Astra High/Extra High | Mesmos eventos, poses, pontos de ligação e efeitos em vídeo comparativo. |
| 3 | Remover aproximações de uma classe/estado por vez | Alta | GPT-6.1 Sol High; Astra para interações/fórmulas ambíguas | Casos numéricos derivados do original e ciclo completo de uso. |
| 4 | Instâncias de itens e serviços econômicos restantes | Alta | GPT-6.1 Sol High; Astra para Creat_Item e save transacional | Recompensas/UID/probabilidades/salvamento consistentes. |
| 5 | Missões e segunda promoção de uma classe | Alta | Astra para contrato Lua; Sol High para casos delimitados | Cadeia completa, ramificações e rejeições corretas. |
| 6 | Estado e ciclo completo de uma embarcação | Muito alta | Astra High/Extra High na arquitetura; Sol High por operação | Comprar/construir, navegar, lutar, reparar e persistir. |
| 7 | Auditar/recuperar recursos por mapa | Baixa a média | Luna High para inventário/mapeamento; Sol para integração | Referências resolvidas com origem comprovada e cena inspecionada. |
| 8 | Fluxos de UI e áudio já especificados | Baixa a média | Sol Medium; Luna High para ajustes bem delimitados | Teste de interação e captura na resolução alvo. |
| 9 | Empacotamento e checks de documentação/build | Baixa a média | Luna High ou Sol Medium | Instalação limpa reproduzível com recursos disponíveis. |
| Contínua | Revisar mudanças de alto risco | Alta | Astra High em revisão independente | Comparação com fonte antiga e testes; sem depender só da explicação da implementação. |

Não é necessário usar esforço máximo em tudo. Um modelo mais forte não recupera sozinho referência visual ausente. O melhor investimento é dar fontes exatas, comportamento esperado e um caso de aceite pequeno. Use Sol como padrão; reserve Astra para incerteza e interações profundas. Luna rende mais quando a regra já está decidida.

## Organização recomendada para manutenção

Manter C++17 e extração incremental. Já existem top_ui, top_item, mission/, npc/ e a divisão de Play em play/; aproveitar essa direção. Evitar uma reescrita geral do motor neste momento.

Responsabilidades sugeridas (estrutura alvo, não descrição de pastas já criadas):

```text
Engine/
  include/top/          contratos públicos pequenos
  src/assets/          leitores e validação dos formatos históricos
  src/world/           mapas, colisão, portais, população
  src/combat/          regras numéricas, estados, resolução de golpes
  src/animation/       poses, relógio e eventos de keyframe
  src/effects/         recursos, instâncias e pontos de ligação
  src/item/            inventário, instâncias, geração, economia
  src/mission/         catálogo, condições, eventos e recompensas
  src/npc/             diálogos e serviços
  src/naval/           barcos e suas regras próprias
  src/persistence/     serialização, versões, validação e recuperação
  src/gl/              desenho e recursos da GPU
  ui/                  apresentação e comandos do jogador
Platform/linux/        janela, entrada e composição da aplicação
tests/                 regressões por domínio e cenários integrados
docs/                  fontes históricas, decisões e matriz de equivalência
```

- **Regra pura separada da orquestração.** Fórmula de dano recebe atributos/estado e retorna resultado; não lê arquivo, não chama GL e não altera UI.
- **Catálogo imutável separado de estado vivo.** SkillDefinition descreve a habilidade; SkillInstance/StatusInstance guarda nível, alvo, tempo e origem de uma execução.
- **Eventos explícitos de animação e combate.** ActionStarted, KeyframeReached, ProjectileLaunched, ImpactResolved e ActionCancelled devem compartilhar identidade/ciclo. A apresentação consome esses dados; não recalcula o dano.
- **A hierarquia de eventos deve respeitar o original.** Alguns golpes resolvem no quadro de ação; outros após o voo. Não impor um único instante de dano a todas as skills.
- **Pontos de ligação como contrato.** O renderer resolve a matriz de corpo/arma/dummy para efeitos. Evitar posições mágicas como chão+0,9 espalhadas pelo sistema.
- **Estado com nomes corretos.** PlayWorld.cpp ainda usa player_.homeX/homeY para máximo/valor de SP. Separar ResourcePool de posição/home antes de expandir o domínio.
- **Adapters nas bordas.** UI emite comandos; gameplay devolve estado para apresentação; save traduz esse estado para formato versionado. Play pode permanecer uma fachada temporária.
- **Uma fonte de verdade para regras.** Guardar ID/callback, arquivo/função original, versão dos dados, suporte e teste. Não replicar fórmulas em UI, combate e animação.
- **Extração de Lua por subconjuntos explícitos.** Ampliar apenas os comandos comprovados e diagnosticar os desconhecidos. Um interpretador Lua embutido seria uma decisão arquitetural separada, com API local bem delimitada; não é pré-requisito imediato.
- **Tratar operações econômicas como transações.** Validar, simular, confirmar e salvar; nenhuma recompensa parcial por falta de espaço.
- **Refatorar junto da funcionalidade.** ActorSkin.cpp e main.cpp ainda concentram muitas responsabilidades. Extrair catálogo/relógio/skinning/preview e aplicação/input/renderização em etapas pequenas, mantendo equivalência.
- **CMake por bibliotecas e função de testes.** Reduzir listas repetidas de fontes nos alvos de teste, manter verificação ativa em Release e tirar testes grandes do executável de produção gradualmente.
- **Distinguir código ativo de cópia antiga.** Engine/src/top/NpcSystem.cpp não é o NpcSystem compilado por CMake; o alvo usa src/npc/NpcSystem.cpp e src/mission/MissionCatalog.cpp. Revisar/remover a duplicação em tarefa própria. Esta limpeza de documentação não remove código.
- **Nomes por domínio, não por migração.** Novos testes devem explicar o comportamento; manter os registros MIGRATION como histórico. Evitar novas camadas genéricas sem necessidade concreta.

## Como transformar a porcentagem em acompanhamento confiável

Criar uma matriz por comportamento, com: domínio, ID/callback, referência original, versão dos dados, status, teste automatizado, evidência visual, risco e próxima ação.

Status recomendados: não inventariado; ausente; aproximado; parcial; fiel em teste; fiel validado em jogo. “Carregado” deve ser uma coluna de recursos, não sinônimo de “portado”.

Para cada tarefa, entregar:
1. Regra original e exemplo reproduzível.
2. Alteração delimitada em um domínio.
3. Teste com expectativa derivada da fonte histórica, inclusive rejeição/erro.
4. Validação visual/interativa quando houver efeito na tela.
5. Atualização da matriz e deste documento, sem criar outro diagnóstico concorrente.

O próximo marco útil é uma experiência pequena completa: criar personagem, jogar uma classe, usar skills fiéis, concluir missões, comprar/equipar, sair e voltar, chegar à costa e navegar. Consolidar essa experiência dará uma medida melhor do que acrescentar centenas de registros parcialmente suportados.

## Limpeza documental desta revisão

Removidos os diagnósticos/roteiros superados: ANALISE_PORTE_GL_2026-10-23, LEVANTAMENTO_05-S, ambos ROADMAP_POST_MISSOES, PROMPT_PROXIMA_MIGRACAO, os resumos antigos ITEM_ACTION_AUDIT_05-AW e ITEM_CHEST_AUDIT_05-AX; e, em offline/docs, auditoria-porte, falta-porte, levantamento, decisao-grafico, escopo-reescrito, skill-spiritual-bolt e opengl-port.

O guia foi atualizado para apontar para este documento. A especificação de formato de mapa e a tabela de tickets foram preservadas como referências específicas. Os MIGRATION_*.md e CSVs antigos permanecem como registros datados; não usar suas contagens/status como cobertura atual. Fontes originais, assets e código de gameplay não foram removidos nesta revisão.

## SK2.17-D — RANGE2 (2026-10-25)

Implementado modo experimental atrás de `TOP_HISTORICAL_RANGE2_COLLISION=1` (também requer `TOP_HISTORICAL_PARTICLE_EXTENDED=1`). No DX9, RANGE2 normaliza a direção, move por `_fVecl * dt`, testa terreno/limites e chama `SendResMessage` ao colidir (`arquivo/fontes/Engine/src/MPParticleSys.cpp:2218-2245`). O porte agora lê `hitEffect` v11+, carrega o `.par` indicado e cria a instância de impacto (`sources/Engine/src/top/EffectSystem.cpp:214-216,572-594,1197-1227`). O slot não é reciclado após o impacto nesta etapa; RANGE2 permanece parcial.

O teste de Client encontrou 35 sistemas RANGE2 em `.par`, todos com nome `hitEffect`; não comprova execução das skills. A colisão com mapa continua aproximada: o DX9 consulta vértices de subgrade (`MPMap.h:346-356`), enquanto o porte usa a altura interpolada disponível em `MapFile::heightAt`. Os dois testes SK2.17-D passaram; a suíte completa passou 103/103 na repetição final. A primeira execução da suíte teve uma falha transitória isolada no FX audit, que passou isoladamente e na repetição. Não houve comparação visual; RANGE2 não está validado visualmente.

Detalhes, limitações e comandos estão em `MIGRATION_SK2_17D_RANGE2_COLLISION.md`. Próxima lacuna RANGE2: reciclagem de slots, aplicada aos 7 skills identificados pelo audit.

## SK2.17-E — RANGE2 pool recycling (2026-10-25)

O controlador de RANGE2 agora recompõe gerações de slots após colisão e dispara cada impacto uma vez por geração, enquanto o emissor ainda está ativo. Evidência DX9: `_CreateRange2` incrementa timer compartilhado por slot morto e `_FrameMoveRange2` volta a chamar a criação enquanto não parado (`arquivo/fontes/Engine/src/MPParticleSys.cpp:2142-2156,2256-2259`). A simulação conserva a ordem, mas usa relógio nominal de 60 Hz; o terreno continua limitado à interpolação existente em `MapFile::heightAt`.

Build Debug compilou `top_gl`; CTest passou 104/104. Três testes RANGE2 passaram sob ASan/UBSan com LSan desativado por incompatibilidade do ambiente com `ptrace`. Não houve comparação visual. Os sete skills RANGE2 medidos continuam pendentes de validação visual; não marcar como fiel/validado.

Detalhes em `MIGRATION_SK2_17E_RANGE2_POOL_RECYCLE.md`; patch incremental: `SK217E.patch`.

## SK2.17-F — RANGE2 subgrid ground height (2026-10-25)

Foi adicionada uma amostragem opcional de `btBlock[0]` para a colisão RANGE2, correspondente ao `GetGridHeight((int)x*2,(int)y*2)` do DX9; a consulta interpolada de `cHeight` continua disponível quando `TOP_HISTORICAL_RANGE2_SUBGRID_HEIGHT` não está em `1`. Build Debug completo, `top_gl` e CTest 104/104 passaram; fixture também passou com ASan/UBSan (LSan desativado). Sem comparação visual com DX9: continua aproximado e precisa da validação em terreno com `btBlock` distinto de `cHeight`.

Detalhes em `MIGRATION_SK2_17F_RANGE2_SUBGRID_HEIGHT.md`; patch incremental: `SK217F.patch`.

## SK2.17-G — decoder `.eff` com parâmetros de modelo não finitos (2026-10-25)

O DX9 grava/lê `height/topRadius/bottomRadius` sem validar finitude (`I_Effect.cpp:517-520,662-669`), enquanto o parser GL rejeitava qualquer `NaN/Inf`. O modo opt-in `TOP_HISTORICAL_EFF_RAW_MODEL_PARAMS=1` aceita valores brutos apenas nesses três campos; keyframes/UV/tempos continuam estritos, e Cone/Cylinder continua validado antes de gerar geometria. Default permanece desligado. O audit do Client agora estrutura 2.832 partes `.eff` e não registra falhas; o audit de blend decodifica todos os 6.308 registros.

Dos 31 `.eff` anteriormente recusados, 30 são referenciados por 32 registros `model_eff` em `.par` válidos. `fboomerff.eff` não tem ligação direta identificada, mas o loader DX9 pré-carrega/indexa todos os `.eff`; seu uso em runtime é inconclusivo, não comprovadamente ausente. Detalhes, limitações e comando visual em `MIGRATION_SK2_17G_EFF_RAW_MODEL_PARAMS.md`. A comparação visual ainda não foi feita; não declarar fidelidade.

Build Debug e CTest completos passaram (**104/104**); fixture focal também passou com ASan/UBSan e LSan desativado. `SK217G.patch` foi aplicado após `git apply --check` em cópia reconstruída da base com SK2.17-A…F; os oito arquivos do patch coincidem com essa cópia e o reverse check passou.

## SK2.17-H — FIRE path-end hit event (2026-10-25)

Adicionado o evento histórico FIRE com caminho e mapa, atrás de `TOP_HISTORICAL_FIRE_HIT_EVENT=1` e `TOP_HISTORICAL_PARTICLE_POOL=1`, ambos opt-in. Os cinco sistemas FIRE do `01010002.par` têm caminho e `hitEffect`; ao primeiro fechamento calculado, o porte impede novas emissões e mantém as partículas já nascidas até o fim da vida. O nome `fire(a).par` não existe no Client examinado e o DX9 também retorna sem instanciar ao não encontrar o controlador; portanto não há efeito visual de impacto neste conjunto de dados. Nenhum substituto foi criado. O ramo sem mapa não foi extrapolado.

Build Debug compilou `top_gl`; CTest passou 105/105; fixture focal passou com ASan/UBSan (LSan desligado). Sem comparação visual com DX9. Relatório e checklist em `MIGRATION_SK2_17H_FIRE_HIT_EVENT.md`; patch incremental `SK217H.patch`.

## SK2.17-I — contagem autorada SNOW/FIRE (2026-10-25)

O DX9 cria/itera `_iParNum` sem o teto 24; o GL limitava os billboards a 24. O audit atualizado agora detecta `count_over_24`: 139 sistemas em 74 `.par`, vinculados a 44 skills; 85 SNOW e 54 FIRE, com 3.647 slots autorados acima do teto somados por sistema. `TOP_HISTORICAL_PARTICLE_COUNT=1` usa a contagem completa somente nos tipos SNOW/FIRE; padrão e demais tipos preservam o teto atual.

Build Debug compilou `top_gl`; CTest passou 106/106; fixture focal passou com ASan/UBSan (LSan desligado); audit Client `drift=0`, `failures=0`. Sem comparação visual. Detalhes em `MIGRATION_SK2_17I_SNOW_FIRE_PARTICLE_COUNT.md`; patch incremental `SK217I.patch`.

## SK2.17-J — timer compartilhado SNOW/FIRE (2026-10-25)

O DX9 incrementa `_fCurTime` por tentativa em cada slot morto; o replay por `slot*step` do porte não correspondia à ordem por frame. O pool opt-in agora recompõe o timer compartilhado e a ordem dos slots num relógio nominal de 60 Hz, incluindo reset do timer em criação rejeitada Snow. O audit atualizado confirma 588 sistemas com `step>0` em 123 skills únicas.

O replay novo fica desligado por padrão e requer `TOP_HISTORICAL_PARTICLE_POOL=1` e `TOP_HISTORICAL_PARTICLE_EMITTER_TIMER=1`. Esse segundo switch preserva a integração anterior do pool: os testes SK2.16-A/B inicialmente detectaram que ligar o algoritmo novo ao switch antigo quebrava suas expectativas; mantivemos os testes originais e isolamos a política nova.

Build Debug completo compilou `top_gl`; CTest passou **107/107** depois do isolamento. O teste focal passou com ASan/UBSan (`detect_leaks=0`). O audit Client terminou com `drift=0`, `failures=0`. Sem comparação visual; relógio, RNG e custo sob muitos efeitos ainda não validados. Relatório: `MIGRATION_SK2_17J_SNOW_FIRE_EMISSION_TIMER.md`; patch incremental: `SK217J.patch`.

## SK2.17-K — correção da classificação de filtros no FX audit (2026-10-25)

O ranking exibia 3.470 sistemas com filtros como lacuna, mas o caminho já existe desde SK2.15-C. O DX9 aplica `_eMinFilter/_eMagFilter` por sistema `.par` (`arquivo/fontes/Engine/src/MPParticleSys.cpp:829-830,1311-1312,1390-1391`); o GL preserva o par e, com `TOP_HISTORICAL_FILTERS=1`, mapeia POINT/LINEAR para `GL_NEAREST/GL_LINEAR` usando sampler objects (`sources/Engine/include/top/HistoricalTextureFilter.h:8-21`; `sources/Engine/src/gl/GfxDeviceGL.cpp:272-290,484-489`). Corrigimos o token e a nota do audit, retiramos filtros do ranking de lacunas e adicionamos teste que exige encontrar o uso no renderer. A auditoria Client atualizou a contagem de lacunas de 21 para 20, com `drift=0`, `failures=0`; os filtros continuam visíveis na tabela de cobertura e marcados sem verificação visual. Nenhuma regra de desenho foi alterada. Detalhes e comandos em `MIGRATION_SK2_17K_FX_FILTER_AUDIT.md`; patch incremental `SK217K.patch`.

## SK2.17-L — limite estrito de amostragem CMPStrip (2026-10-25)

O DX9 chama `GetTrack` quando `_fCurTime > _fStep` (`arquivo/fontes/Engine/src/MPModelEff.cpp:1296-1310`); o GL aceitava `elapsed == step`. O novo switch `TOP_HISTORICAL_CMPSTRIP_STRICT_STEP=1`, desligado por padrão, reproduz a condição estrita sem mudar o default. Build Debug completo e `top_gl` passaram; CTest passou **107/107**; a fixture CMPStrip passou com ASan/UBSan (`detect_leaks=0`). O audit Client registrou seis faixas diretas e zero falhas de decodificação. Sem comparação visual; a etapa não resolve a lacuna `direct_cmpstrip` de anchors não cobertos, ainda medida em 16 skills. Relatório: `MIGRATION_SK2_17L_CMPSTRIP_STRICT_STEP.md`; patch incremental: `SK217L.patch`.

## SK2.17-M — grade de SHADE projetada no terreno (2026-10-25)

O audit mediu 8 sistemas `type=13` em 8 `.par`, alcançando 16 skills; o DX9 envia SHADE a `CMPShadeCtrl::Render` e compõe uma grade de terreno em `MPShadeMap.cpp`, enquanto o GL anterior caía no quad genérico. `TOP_HISTORICAL_SHADE=1` agora gera a grade de triângulos e seus UV com o raio inicial autorado. Na borda DX9→GL, converte plano XY com Z-up para XZ com Y-up, usa cor de vértice como equivalente ao TFACTOR e mantém o par de blend do `.par` no renderer GL.

A amostragem GL usa `MapFile::heightAt`, que não expõe separadamente altura de objetos nem o teste de célula bloqueada que o DX9 usa. SHADE continua aproximado e sem validação visual. Build Debug completo e `top_gl` passaram; CTest passou **109/109**; os fixtures unitário e integrado passaram com ASan/UBSan (`detect_leaks=0`). O audit atualizado decodificou 1.031 `.par`, 1.227 `.eff`, 3.476 sistemas e 2.832 partes `.eff`, com zero falhas de decodificação e `drift=0`. A classificação da tabela de auditoria mudou de `missing_specialization` para `partial`; o tipo permanece como lacuna por causa das diferenças de terreno e validação visual.

Detalhes e comparação visual em `MIGRATION_SK2_17M_SHADE_TERRAIN_GRID.md`; patch incremental: `SK217M.patch`.

## SK2.17-N — override de alfa do STRIP aninhado (2026-10-25)

Auditoria comparando `.par` type 6 e `.eff` encontrou 84 controladores com alfa inicial do pai abaixo de 1; todos os filhos foram decodificados e 33 sistemas têm alfa filho não unitário. Os 84 estão em `sceneffectinfo`; nenhum dos 33 divergentes liga diretamente a um canal de skill em `skillinfo`. O único sistema com canal direto alcança quatro skills (Fiery Arrow, Inferno Blast, Parry e Dispersion Bullet), mas seu `ice.eff` tem alfa unitário e não diverge. O visual ID 200 (“Frozen Arrow”) referencia `01020033.par` em `sceneffectinfo`, sem canal direto demonstrado em `skillinfo`. O DX9 chama `CMPModelEff::SetAlpha` nas condições medidas, substituindo só alfa (`arquivo/fontes/Engine/src/MPParticleSys.cpp:1573-1575`; `arquivo/fontes/Engine/include/MPModelEff.h:596-602`); o GL multiplicava RGBA (`EffectSystem.cpp:2014-2019`). `TOP_HISTORICAL_STRIP_PARENT_ALPHA=1` implementa o override no alfa de vértice, preservando RGB, desligado por padrão. O auditor `top_strip_alpha_audit` confirmou zero filhos `.eff` não estruturados e zero falhas `.par`; o FX audit também tem zero recusas `.eff`. Build Debug de `top_gl` e auditor concluído; CTest **111/111**. Fixtures cobrem os caminhos default e histórico. Sem comparação visual DX9/GL. Relatório: `MIGRATION_SK2_17N_STRIP_PARENT_ALPHA.md`; patch: `SK217N.patch`.

## SK2.17-O — aceleração discreta de BLAST (2026-10-25)

O ranking mede 423 sistemas type 3 em 118 `.par`/101 skills; 208 sistemas com aceleração não nula ocupam 66 arquivos e alcançam 64 skills. No DX9, BLAST atualiza posição com a velocidade corrente e só depois integra aceleração (`arquivo/fontes/Engine/src/MPParticleSys.cpp:3923-3926`). O porte usava `0.5*a*t²`; `TOP_HISTORICAL_BLAST_DISCRETE_ACCELERATION=1`, desligado por padrão e condicionado a `TOP_HISTORICAL_PARTICLE_POOL=1` + `TOP_HISTORICAL_PARTICLE_MOTION=1`, corrige essa diferença para o relógio nominal de 60 Hz. O FX audit agora mede `feature:blast_discrete_acceleration`; run no Client: 208 sistemas/66 arquivos/64 skills, drift 0/falhas 0. Build Debug de `top_gl` e do audit; CTest **113/113**; fixtures O e FX audit passaram; ASan/UBSan nos fixtures O passou. RNG e dt real DX9 continuam aproximados; sem comparação visual. Relatório: `MIGRATION_SK2_17O_BLAST_DISCRETE_ACCELERATION.md`; patch incremental: `SK217O.patch`.

## SK2.17-P — movimento e colisão do SHRINK (2026-10-25)

O ranking do FX audit mede 42 sistemas type 12 em 32 `.par`, alcançando 53 skills. O DX9 sorteia coordenadas em `[0, range]`, move na direção normalizada ao centro com a velocidade do `.par` e encerra após `PointPointRange` verificar `abs(delta eixo)<0.5` (`arquivo/fontes/Engine/src/MPParticleSys.cpp:2439-2446,2518-2521`; `arquivo/fontes/Engine/include/MPParticleSys.h:54-58`). O GL anterior usava fatores `.22/.18/.12` e um limite radial `.06` (`EffectSystem.cpp:2204-2215`). `TOP_HISTORICAL_SHRINK=1` recupera o movimento, a caixa de colisão e a idade de curva individual em replay nominal de 60 Hz, mantendo o default. Build Debug completo incluindo `top_gl`; CTest **115/115**; fixtures P unitário e integrado passaram. O Client auditou 1.031 `.par`/1.227 `.eff`, com 0 falhas de decodificação, 21 lacunas e 307 skills alcançadas por ao menos uma lacuna. A amostra aleatória, `dt`, emissão/reciclagem e escala ainda são aproximações; sem comparação visual. Relatório: `MIGRATION_SK2_17P_SHRINK_COLLISION.md`; patch incremental: `SK217P.patch`.

## SK2.17-Q — timer SNOW/FIRE por `dt` real (2026-10-25)

O ranking mede `snow_fire_step` em 588 sistemas/266 `.par`/123 skills. No DX9, `_CreateSnow`/`_CreateFire` somam `*_pfDailTime` a um único `_fCurTime`, zeram o timer ao atingir `_fStep`, e cada `FrameMove` visita slots em ordem (`arquivo/fontes/Engine/src/MPParticleSys.cpp:3462-3468,3579-3597,3681-3687,3752-3832`). O replay anterior usava nominal 60 Hz. `SystemTimeline`, atrás de `TOP_HISTORICAL_PARTICLE_POOL=1` + `TOP_HISTORICAL_PARTICLE_EMITTER_TIMER=1`, agora avança por `EffectSystem::step(dt)` real, com timer compartilhado, frames/vidas por slot e sem reutilizar um slot recém-morto na mesma chamada. O audit foi atualizado para validar `snowFireTimelines`; no Client: 1.031 `.par`, 1.227 `.eff`, 3.476 sistemas, 2.832 partes `.eff`, zero falhas/deriva, 21 lacunas e 307 skills alcançadas. Sorteio global `rand()`, direção/movimento aleatórios e parada de FireHit continuam aproximados. Build Debug completo (incluindo `top_gl`) concluído; CTest **117/117**; fixtures Q unitário/integrado passaram com ASan/UBSan (`detect_leaks=0`). Audit Client terminou com `drift=0`, `failures=0`; `fx_decode_failures.csv` não contém `.eff` recusado. Sem comparação visual. Relatório: `MIGRATION_SK2_17Q_SNOW_FIRE_ACTUAL_DT.md`; patch: `SK217Q.patch`.

## SK2.17-R — temporização dos quadros dos filhos `.eff` em STRIP (2026-10-25)

O audit mede 1.041 sistemas type 6 em 504 `.par`; `top_par_audit` confirmou 875 que referenciam filhos `.eff` em 500 `.par`, alcançando 242 skills. DX9 `CMPModelEff::FrameMove` avança no máximo uma chave e zera o residual (`arquivo/fontes/Engine/src/MPModelEff.cpp:500-540`); GL `legacy_eff::framePhase` derivava a fase da idade total (`sources/Engine/include/top/LegacyEffFormat.h:378-401`). `TOP_HISTORICAL_STRIP_EFF_FRAME_STEP=1` ativa estado por subefeito alimentado por `EffectSystem::step(dt)`; o grupo só reinicia quando todos os filhos terminam, usando a política anterior `TOP_STRIP_NESTED_LOOP`. A amostragem da geometria usa a chave explícita; UV e textura seguem por idade analítica.

Build Debug completo compilou `top_gl`; CTest **119/119**. Testes R unitário, integrado e regressão do FX audit passaram com ASan/UBSan (`detect_leaks=0`). FX audit: 1.031 `.par`, 1.227 `.eff`, 3.476 sistemas, 2.832 partes `.eff`, 875 STRIP `.eff`, 22 lacunas, `drift=0`, `failures=0`. Sem comparação visual; não validado como fiel. Relatório: `MIGRATION_SK2_17R_STRIP_EFF_FRAME_STEP.md`; patch incremental: `SK217R.patch`.

## SK2.17-S — clocks de UV e textura em filhos STRIP `.eff` (2026-10-25)

O DX9 despacha os tipos `EFFECT_FRAMETEX`, `EFFECT_MODELUV` e `EFFECT_MODELTEXTURE` para clocks independentes por controlador (`arquivo/fontes/Engine/src/MPModelEff.cpp:568-589`); os três avançam uma vez quando `elapsed > frameTime` e descartam o residual (`arquivo/fontes/Engine/src/I_Effect.cpp:2403-2421,2552-2560,2631-2639`). GL podia saltar por tempo acumulado. `TOP_HISTORICAL_STRIP_EFF_UV_CLOCK=1` adiciona estados por filho e roteia cada pista pelo tipo DX9; default preservado. Audit Client final: 1.031 `.par`, 1.227 `.eff`, 875 STRIP com `.eff`, `failures=0`, então nenhum `.eff` falhou e não houve recurso recusado para triagem. Build Debug completo incluindo `top_gl`; CTest **121/121**; testes S unitário e integrado passaram; unitário também passou com ASan/UBSan (`detect_leaks=0`). `IsItem` não tem flag equivalente preservada no decoder, e nenhum comparativo visual foi feito. Relatório: `MIGRATION_SK2_17S_STRIP_EFF_UV_TEXTURE_CLOCKS.md`; patch incremental: `SK217S.patch`.

## SK2.17-T — cobertura de uso dos filhos `.eff` STRIP e gate `IsItem` (2026-10-25)

`top_strip_eff_animation_audit` cruza referências `.eff` explícitas de type 6, valida enum/dispatch nos fontes DX9 e registra subefeitos por tipo. No Client: 875 referências em 500 `.par`, 568 `.eff` únicos decodificados, 0 faltantes/falhas; dos 973 subefeitos tipos 1–3, 2 `FRAMETEX`, 4 `MODELUV` e 367 `MODELTEXTURE` têm mais de uma entrada. Os 973 modelos são primitivos nativos; pelas rotas de `GetMeshByName`/`CreateTob` e pelo teste `_lpVB`/`_lwMesh`, `IsItem` é falso nesses modelos, então o gate omitido pelo decoder não altera esses dados. A auditoria geral confirmou os 1.227 `.eff` do Client sem falhas. O fixture também prova que `.eff` corrompido referenciado aparece como falha referenciada. Build Debug completo compilou `top_gl` e o novo audit; CTest **122/122**. Nenhum comparativo visual foi feito. Relatório: `MIGRATION_SK2_17T_STRIP_EFF_USAGE_AUDIT.md`; patch incremental: `SK217T.patch`.

## SK2.17-U — aceleração aleatória SNOW/FIRE por `dt` real (2026-10-25)

O FX audit Client mediu 7 sistemas com aceleração SNOW/FIRE não nula em 5 `.par`; o grafo atual não encontra vínculo direto com skills. DX9 aplica `±accel*dt` com um `rand()%2` por atualização viva (`arquivo/fontes/Engine/src/MPParticleSys.cpp:3610-3618,3783-3789`). `TOP_HISTORICAL_PARTICLE_FRAME_ACCELERATION=1` acumula o termo por slot/generation usando o `dt` real da timeline; continua determinístico por slot e não reproduz o `rand()` global. Requer pool, timer de emissor e movimento histórico; default desligado. Build Debug completo compilou `top_gl`; CTest **122/122**; fixture Q passou com ASan/UBSan (`detect_leaks=0`). Audit: 1.031 `.par`, 1.227 `.eff`, 3.476 sistemas, zero deriva/falhas; arquivo `fx_decode_failures.csv` só tem cabeçalho. Não houve comparação visual. Relatório: `MIGRATION_SK2_17U_SNOW_FIRE_FRAME_ACCELERATION.md`; patch incremental: `SK217U.patch`.

## SK2.17-V — auditoria dos proprietários de CMPStrip direto (2026-10-25)

`top_direct_strip_anchor_audit` decodifica os trailers direto dos `.par`, liga registros a canais/skills e inspeciona callsites. Client: 1.031 `.par`, seis CMPStrip em seis arquivos, 16 IDs de skill, zero falhas; um trailer sem linha de dependência. DX9 `CMPStrip::Play()` requer `_pItem` ou `_pCha` (`arquivo/fontes/Engine/src/MPModelEff.cpp:1251-1272`); a medição V listou `SetStripItem` em `MPEffectCtrl.cpp`, mas omitiu o chamador `EffectObj.cpp`. A conclusão V de que não havia callsite `.SetStripCharacter(...)` foi corrigida em SK2.17-BB: `Client/src/EffectObj.cpp:1048-1060` liga ambos os owners condicionalmente. GL só tem provedores do corpo e item equipado do jogador (`EffectSystem.cpp:1389-1396`; `ActorSkin.cpp:1408-1468`). Não houve mudança de renderer por falta de evidência de owner runtime e por ausência de matrizes de outros personagens. Build Debug completo; CTest 123/123; fixture passou com ASan/UBSan (`detect_leaks=0`). CMPStrip segue parcial e sem validação visual. Relatório: `MIGRATION_SK2_17V_DIRECT_STRIP_OWNER_AUDIT.md`; patch incremental `SK217V.patch`.

## SK2.17-W — avanço discreto de curvas MODEL (2026-10-25)

O FX audit mede 777 sistemas `type=5` em 387 `.par`; 433 sistemas em 211 arquivos têm mais de um quadro e alcançam 54 skills. DX9 `GetCurFrame` avança no máximo um quadro e descarta o residual (`arquivo/fontes/Engine/include/MPParticleSys.h:116-129`); `_CreateModel` define vida/contagem de quadros e reinicia o cursor (`arquivo/fontes/Engine/src/MPParticleSys.cpp:3074-3087`), e `_FrameMoveModel` retorna sem recalcular a curva no update de reinício (`:3101-3110`). GL usava `fmod(particleAge, profile.life)` (`sources/Engine/src/top/EffectSystem.cpp:1825-1835`). `TOP_HISTORICAL_MODEL_FRAME_STEP=1` agora ativa um timeline discreto por instância/perfil, recebendo `step(dt)` e conservando a última amostra no update de reinício; default desligado. O FX audit mede a feature pelo conteúdo decodificado: 433 sistemas/211 arquivos/54 skills. Audit Client: 1.031 `.par`, 1.227 `.eff`, 3.476 sistemas, 2.832 partes, zero falhas/deriva; 24 lacunas, 307 skills alcançadas. Build Debug completo com `top_gl`; CTest **124/124**; fixture W passou com ASan/UBSan (`detect_leaks=0`). Sem comparação visual; cadência e aspecto em runtime ainda não validados. Relatório `MIGRATION_SK2_17W_MODEL_FRAME_STEP.md`; patch incremental `SK217W.patch`.

## SK2.17-X — ciclo de emissão/reciclagem RIPPLE (2026-10-25)

O audit encontrou 23 sistemas `type=4` em 12 arquivos; 21 têm `step>0` e múltiplas chaves, em 11 arquivos/21 skills. No DX9, `Play` inicia `_fCurTime` em `_fStep` (`arquivo/fontes/Engine/src/MPParticleSys.cpp:885-894`); `_CreateRipple` soma `dt`, zera no intervalo e atribui vida aleatória entre `_fLife/_iRoadom` e `_fLife`; `_FrameMoveRipple` percorre slots, avança quadro uma vez e só reusa um slot morto na próxima chamada (`:3967-3997,4013-4045`). O GL anterior usava idades analíticas por índice e não reciclava type 4. `TOP_HISTORICAL_RIPPLE_POOL=1` agora ativa timer compartilhado, estado de vida/quadro por slot, sorteio determinístico usando o divisor `.par` e reciclagem em atualização posterior; default desligado. Os 23 recursos decodificados não têm `model_range` nem `has_path`; sete têm `.eff` filho, cujo relógio continua analítico no GL e alcança seis skills; `IsBone` não é coberto pelo audit. O RNG global e geometria continuam sem validação visual. Audit Client: 1.031 `.par`, 1.227 `.eff`, 3.476 sistemas, 2.832 partes, zero falhas/deriva; 26 lacunas, 307 skills alcançadas. Build Debug completo com `top_gl`; CTest **126/126**; testes X unitário/integrado e audit passaram; unitário passou com ASan/UBSan (`detect_leaks=0`). Relatório `MIGRATION_SK2_17X_RIPPLE_EMISSION_RECYCLE.md`; patch incremental `SK217X.patch`.

## SK2.17-Y — relógio dos `.eff` filhos RIPPLE (2026-10-25)

O DX9 chama `CMPModelEff::FrameMove(dt)` uma vez em `_FrameMoveRipple` (`arquivo/fontes/Engine/src/MPParticleSys.cpp:4007-4018`); cada filho avança no máximo um quadro, zera o residual e mantém seu próprio controlador (`arquivo/fontes/Engine/src/MPModelEff.cpp:514-540,548-589`). O GL derivava a fase do `modelAge` analítico (`sources/Engine/src/top/EffectSystem.cpp:2162-2169`). Agora mantém uma timeline de grupo por perfil RIPPLE, reiniciada em loop porque o original chama `Play(0)` (`MPParticleSys.cpp:885-905`; `MPModelEff.h:377-404`), e usa essa timeline em quadro, textura e UV sob `TOP_HISTORICAL_RIPPLE_POOL=1`; continua opt-in. O Client audit desta etapa registrou sete sistemas em sete PARs/seis skills e quatro EFFs filhos distintos decodificados; a auditoria geral encontrou zero falhas nos 1.227 EFFs. Resultado novo: 25 lacunas e 307 skills com lacunas; a feature de quadros EFF filhos RIPPLE saiu do ranking. Build Debug com `top_gl`; CTest **127/127**. Sem comparação visual com DX9. Relatório `MIGRATION_SK2_17Y_RIPPLE_CHILD_EFF_TIMELINE.md`; patch incremental `SK217Y.patch`.

## SK2.17-Z — correção da classificação STRIP no FX audit (2026-10-25)

O audit contava 875 sistemas STRIP com EFF (500 PARs/242 skills) como lacuna e dizia que quadros, textura e UV permaneciam aproximados. DX9 avança cada subefeito uma etapa e atualiza o controlador (`arquivo/fontes/Engine/src/MPModelEff.cpp:514-540,568-589`); o porte já tem frame/group timeline e relógios UV opt-in separados (`sources/Engine/include/top/HistoricalEffTimeline.h:11-20,54-97`; `sources/Engine/src/top/EffectSystem.cpp:1081-1088,2122-2129`). A tabela agora descreve os dois switches, e o auditor mantém a feature medida mas não a conta como gap. `ui05sk214_fx_coverage_audit` passou. Nova execução Client: 1.031 PAR, 1.227 EFF, 3.476 sistemas, 2.832 subefeitos, zero falhas/deriva, 24 lacunas e 307 skills com lacunas. STRIP ainda aparece como tipo parcial por outras lacunas, incluindo CMPStrip direto. Sem comparação visual. Relatório `MIGRATION_SK2_17Z_STRIP_EFF_AUDIT_CORRECTION.md`; patch incremental `SK217Z.patch`.

## SK2.17-AA — classificação de contagens SNOW/FIRE (2026-10-25)

DX9 define `_iParNum` em `SetSysNum` e percorre todos os slots em `_FrameMoveSnow`/`_FrameMoveFire` (`arquivo/fontes/Engine/src/MPParticleSys.cpp:269-270,3524-3582,3734-3757`). No GL, `TOP_HISTORICAL_PARTICLE_COUNT=1` seleciona a quantidade SNOW/FIRE serializada (`sources/Engine/include/top/HistoricalParticleCount.h:8-19`); o padrão continua limitado a 24. O audit mediu 139 sistemas acima de 24, em 74 PARs/44 skills. Corrigi a classificação stale para manter a feature na cobertura e removê-la do ranking de gaps; o teste `ui05sk214_fx_coverage_audit` verifica isso. A densidade não foi comparada visualmente com DX9. Relatório `MIGRATION_SK2_17AA_PARTICLE_COUNT_AUDIT.md`; patch incremental `SK217AA.patch`.

A execução completa do audit após AA encontrou 1.031 `.par`, 1.227 `.eff`, 3.476 sistemas e 2.832 subefeitos; zero falhas e deriva, **23** gaps e 307 skills com gaps.

## SK2.17-AB — timeline de quadros ARRAW (2026-10-25)

O audit mede oito sistemas ARRAW em oito `.par`, cada um com referência `.eff` e zero skills diretamente alcançadas pelo grafo atual. O DX9 mantém um slot centralizado e avança seu quadro e o `CMPModelEff` filho uma vez por `FrameMove` (`arquivo/fontes/Engine/include/MPParticleSys.h:193`; `arquivo/fontes/Engine/src/MPParticleSys.cpp:3007-3070`). O GL agora pode usar timelines discretos de curvas e do `.eff` filho por `EffectSystem::step(dt)`, sem aplicar rotação do pai ARRAW não calculada no ramo DX9. Requer `TOP_HISTORICAL_PARTICLE_EXTENDED=1` e `TOP_HISTORICAL_ARRAW_TIMELINE=1`; ambos desligados por padrão.

O audit geral atual encontrou 1.031 `.par`, 1.227 `.eff`, 3.476 sistemas, 2.832 partes `.eff`, zero falhas/deriva, 23 lacunas e 307 skills com lacunas. O CSV de decodificação contém só cabeçalho. Build Debug completo incluindo `top_gl`; CTest **128/128**. Não houve comparação visual nem ASan/UBSan; ARRAW continua aproximado, não validado como fiel. Relatório `MIGRATION_SK2_17AB_ARRAW_EFF_TIMELINE.md`; patch incremental `SK217AB.patch`.

## SK2.17-AC — triagem de falhas estritas de `.eff` (2026-10-25)

O FX audit agora lista falhas estritas por recurso, mede se o modo diagnóstico consegue estruturar o EFF, registra parâmetros não finitos por modelo e cruza referências PAR, IDs de `sceneffectinfo.txt` e o grafo de habilidades. Client atual: 31 falhas estritas, 31 recuperadas com parâmetros brutos e zero falhas diagnósticas; 30 têm PAR pai e ID de cena (32 referências PAR); `fboomerff.eff` não tem referência direta localizada, mas DX9 carrega globalmente todos os `.eff`, então não é comprovadamente inútil. Os 98 parâmetros base não finitos pertencem a `RectPlane` ou modelos LGO; nenhum em `Cylinder`. Build Debug completo com `top_gl`; CTest **128/128**. Uso dinâmico, habilidade alcançada pelo grafo e paridade visual não comprovados. Relatório `MIGRATION_SK2_17AC_EFF_FAILURE_USEFULNESS.md`; patch incremental `SK217AC.patch`.

## SK2.17-AD — timeline discreta do BLAST3 (2026-10-25)

O audit atual mede 1 sistema type 11 com mais de um quadro em 1 `.par`; adiciona a feature `blast3_nested_eff_region_clock` para cruzar pai/filho pelo código. DX9 `_CreateBlast3` distribui `(1,1,-1)` radialmente e soma `_vDir` (`arquivo/fontes/Engine/src/MPParticleSys.cpp:2577-2606`); `_FrameMoveBlast3` avança o filho antes das partículas, move antes da aceleração e encerra o pai ao fim (`:2623-2637,2655-2658`; `arquivo/fontes/Engine/include/MPParticleSys.h:116-129`). `kulou.eff`, referenciado por `fushe.par`, está decodificado: type 3, uma chave, quatro regiões UV a cada 0,1 s. O código GL avança índice e resíduo por `step(dt)`; a fixture integrada confirma a mudança de UVs, sem comparar pixels com DX9. Requer `TOP_HISTORICAL_PARTICLE_EXTENDED=1` e `TOP_HISTORICAL_BLAST3_TIMELINE=1`, defaults desligados. Audit Client: 1.031 PAR, 1.227 EFF, 3.476 sistemas, 2.832 partes EFF, 23 lacunas, zero deriva/falhas; a feature nova mede 1 referência pai/filho e 0 skills rastreáveis. Build Debug completo compilou `top_gl`; CTest **129/129**; fixture integrada passou também com ASan/UBSan (`detect_leaks=0`). Relatório `MIGRATION_SK2_17AD_BLAST3_UPDATE_TIMELINE.md`; patch incremental `SK217AD.patch`.

## SK2.17-AE — aceleração BLAST pelo `dt` efetivo (2026-10-25)

O FX audit mede 208 sistemas BLAST com aceleração não nula em 66 `.par`/64 skills. DX9 move pela velocidade antiga e depois soma `accel*dt` à velocidade (`arquivo/fontes/Engine/src/MPParticleSys.cpp:3923-3926`). O porte usava `0.5*a*t²`; `TOP_HISTORICAL_BLAST_ACTUAL_DT=1`, desligado por padrão e condicionado a `TOP_HISTORICAL_PARTICLE_POOL=1` + `TOP_HISTORICAL_PARTICLE_MOTION=1`, corrige a integração contínua pela soma dos deltas recebidos em `EffectSystem::step`. Testes O verificam acumulação, nova geração e morte; fixture integrada verifica deltas `.2/.1/.2`. Build Debug completo compilou `top_gl`; CTest **129/129**; a fixture passou com ASan/UBSan (`detect_leaks=0`). Audit Client: 1.031 `.par`, 1.227 `.eff`, 3.476 sistemas, 2.832 partes `.eff`, 23 lacunas, zero deriva e zero falhas diagnósticas; o JSON separa 31 falhas estritas de `.eff`, 30 referenciados por PAR/ID de cena e um (`fboomerff.eff`) sem referência direta, mas ainda sem prova de desuso. RNG global DX9, vida/reciclo exatos e comparação visual continuam aproximados/não verificados. Relatório `MIGRATION_SK2_17AE_BLAST_ACTUAL_DT.md`; patch incremental `SK217AE.patch`.

## SK2.17-AF — altura de subgrade e bloqueio no SHADE (2026-10-25)

O audit mede 8 sistemas SHADE em 8 `.par`/16 IDs de skill. DX9 `CMPShadeMap::MoveTo` compara `GetGridHeight` com `GetTileHeight`, exceto em quadrantes bloqueados, e corrige o índice na borda (`arquivo/fontes/Engine/src/MPShadeMap.cpp:389-412`). A grade da SK2.17-M usava apenas `MapFile::heightAt`; `TOP_HISTORICAL_SHADE_TERRAIN_GRID=1`, desligado por padrão e dependente de `TOP_HISTORICAL_SHADE=1`, agora decodifica `btBlock[0..3]`, bit de bloqueio e deslocamento da borda. `.map` sintético, teste de callback, integração GL e fixture do audit passaram. Audit Client: 1.031 PAR, 1.227 EFF, 3.476 sistemas, 2.832 partes EFF, 24 lacunas, zero deriva/falhas estruturais; continuam 31 falhas `.eff` estritas (30 com referência PAR/cena e `fboomerff.eff` sem referência direta; uso deste permanece desconhecido). Build Debug completo incluindo `top_gl`; CTest **129/129**; a fixture `.map` e a integração SHADE passaram com ASan/UBSan (`detect_leaks=0`). A janela de terreno DX9 e paridade visual seguem sem comparação; SHADE permanece aproximado. Relatório `MIGRATION_SK2_17AF_SHADE_SUBGRID_HEIGHT.md`; patch incremental `SK217AF.patch`.

## SK2.17-AG — auditoria de PAR irmão para `.eff` sem referência direta (2026-10-25)

O relatório de utilidade dos `.eff` passa a separar links diretos PAR→EFF da coincidência de nome-base PAR/EFF. No Client, `fboomerff.par` aparece na cena 919 e na skill 308; seus três sistemas usam `RectPlane` e não apontam para `fboomerff.eff`. O DX9 enumera e indexa todos os `.eff` em `CMPResManger::LoadTotalEffect` (`arquivo/fontes/Engine/src/MPResManger.cpp:1351-1405`), então essa nova ligação não prova que o EFF é inútil ou usado em runtime. O audit atualizou `fx_eff_usefulness.csv`; contagens: 1.031 PAR, 1.227 EFF, 3.476 sistemas, 2.832 partes EFF, 24 lacunas, zero deriva e zero falhas diagnósticas; 31 recusas estritas continuam recuperáveis no modo diagnóstico. Teste direcionado `ui05sk214_fx_coverage_audit` passou. A compilação OpenGL completa e CTest completo não foram executados nesta etapa. Relatório `MIGRATION_SK2_17AG_EFF_SIBLING_LINKAGE_AUDIT.md`; patch incremental `SK217AG.patch`.

## SK2.17-AH — lacuna do relógio de colisão RANGE2 (2026-10-25)

Corrigi a classificação obsoleta que dizia “terrain hit callback pending”: colisão, `hitEffect`, reciclagem e altura de subgrade já existem em etapas D/E/F, atrás dos switches registrados nesses relatórios. O audit agora mede separadamente a cadência: DX9 move/testa a colisão a cada `FrameMove(dt)` (`arquivo/fontes/Engine/src/MPParticleSys.cpp:2195-2245`), enquanto o helper GL usa replay nominal de 60 Hz (`sources/Engine/include/top/HistoricalRange2Collision.h:40-46,97-106`; `EffectSystem.cpp:1331-1348`). `range2_frame_update_clock` mede 35 sistemas/19 PARs/7 skills ligados estaticamente. Mudança só no catálogo/auditor; nenhuma regra de runtime foi alterada. Teste direcionado do audit e o audit Client são executados para esta etapa; não rodei `top_gl`, CTest completo, sanitizers ou comparação visual. A timeline real ainda precisa preservar os `dt`, posições de nascimento, timer compartilhado e eventos por slot. Relatório `MIGRATION_SK2_17AH_RANGE2_CLOCK_AUDIT.md`; patch incremental `SK217AH.patch`.

## SK2.17-AI — timeline RANGE2 com `dt` efetivo (2026-10-25)

Adicionada a timeline opt-in `TOP_HISTORICAL_RANGE2_ACTUAL_DT=1`, dependente de `TOP_HISTORICAL_PARTICLE_EXTENDED=1` e `TOP_HISTORICAL_RANGE2_COLLISION=1`. O DX9 atualiza o controlador, move cada slot e testa a colisão uma vez por `FrameMove(dt)` (`arquivo/fontes/Engine/src/MPParticleSys.cpp:2201-2245`); a nova timeline persiste slots e timer compartilhado e usa os deltas guardados por `EffectSystem::step`, consumidos no `build` com o mapa e a origem/path daquela idade. Preserva a tradução de coordenadas DX9 X/Y horizontais, Z vertical para GL X/Z horizontais, Y vertical na borda do renderer. O replay nominal de 60 Hz permanece como fallback; casts presos a dummies móveis também permanecem no caminho anterior porque o porte não armazena o histórico de poses. Build Debug completo incluindo `top_gl` concluído; CTest completo **129/129**; fixtures unitária e integrada passaram com ASan/UBSan (`detect_leaks=0`). Audit Client: 1.031 `.par`, 1.227 `.eff`, 3.476 sistemas, 2.832 partes EFF, 25 lacunas, zero deriva e zero falhas estruturais no modo diagnóstico; ainda há 31 recusas estritas de `.eff`, recuperadas no modo diagnóstico. Não houve comparação visual. Relatório `MIGRATION_SK2_17AI_RANGE2_ACTUAL_DT.md`; patch incremental `SK217AI.patch`.

## SK2.17-AJ — timeline de filhos `.eff` em MODEL (2026-10-25)

O audit atual mede 741 sistemas type 5 em 384 `.par` com modelo `.eff`, ligados estaticamente a 57 skills. No DX9, `_FrameMoveModel` atualiza o filho `CMPModelEff` por `dt` real (`arquivo/fontes/Engine/src/MPParticleSys.cpp:3161-3167`); `CMPModelEff::FrameMove` avança no máximo um quadro e reinicia o grupo se estiver em loop (`arquivo/fontes/Engine/src/MPModelEff.cpp:500-540`). O OpenGL mantém fallback analítico por idade (`sources/Engine/src/top/EffectSystem.cpp:2354-2366`); a nova timeline, ativada junto de `TOP_HISTORICAL_MODEL_FRAME_STEP=1` e `TOP_HISTORICAL_MODEL_EFF_TIMELINE=1`, mantém um grupo por perfil e omite o avanço no passo de wrap do cursor pai. Audit atual: 1.031 `.par`, 1.227 `.eff`, 3.476 sistemas, 2.832 partes EFF, 26 lacunas, drift/falhas estruturais zero. As 31 recusas estritas de `.eff` foram recuperadas no modo diagnóstico; 30 têm referência PAR direta e uma (`fboomerff.eff`) só tem PAR de nome-base, sem prova de uso em runtime. Build Debug completo incluindo `top_gl` concluído; CTest **131/131**; testes novo unitário e integrado passaram com ASan/UBSan (`detect_leaks=0`). A fixture mostra diferença de centroide X=15 no fallback e X=10 na timeline. Sem comparação visual. Relatório `MIGRATION_SK2_17AJ_MODEL_EFF_TIMELINE.md`; patch incremental `SK217AJ.patch`.

## SK2.17-AK — reinício de grupo do BLAST (2026-10-25)

O FX audit mediu 423 sistemas BLAST em 118 `.par`, alcançados por 101 skills. O DX9 incrementa `_wDeath` e só chama `_CreateBlast` quando todos os slots terminaram (`arquivo/fontes/Engine/src/MPParticleSys.cpp:3891-3909`); no caminho `CMagicCtrl`, `Play(0)` faz o grupo repetir (`MPEffectCtrl.cpp:545-548`). A timeline opt-in `TOP_HISTORICAL_BLAST_GROUP_CYCLE=1`, dependente de `TOP_HISTORICAL_PARTICLE_POOL=1`, recria a geração de todos os slots juntos e usa o `dt` das chamadas de `EffectSystem::step`; o `TOP_HISTORICAL_PARTICLE_MOTION=1` aplica as amostras a movimento/posições. Não altera a tradução de coordenadas do renderer nem afirma reprodução do `rand()` global DX9. O audit Client passou com 27 lacunas, `drift=0` e zero falhas estruturais; das 31 recusas estritas `.eff`, 30 são diretamente referenciadas e uma (`fboomerff.eff`) permanece sem uso runtime comprovado. Build Debug incluindo `top_gl`; CTest **133/133**; fixtures novas passaram com ASan/UBSan (`detect_leaks=0`). Sem comparação visual. Relatório `MIGRATION_SK2_17AK_BLAST_GROUP_CYCLE.md`; patch `SK217AK.patch`.

## SK2.17-AL — consistência da flag SHADE no audit (2026-10-25)

Corrigido um falso positivo: DX9 recalcula `m_bShade` a partir do tipo em `SetOpertion` (`arquivo/fontes/Engine/src/MPParticleSys.cpp:361-405`) e a carga em memória chama essa função depois de ler o byte (`:4469-4473`); o renderer GL seleciona SHADE pelo tipo 13 (`sources/Engine/src/top/EffectSystem.cpp:2278-2295`). O audit agora mede somente divergências entre o byte presente (v10+) e o tipo. No Client atual, 8 flags correspondem a 8 sistemas type 13, divergências zero; o ranking cai de 27 para 26 lacunas sem alterar o runtime. Build Debug completo incluindo `top_gl`; CTest **133/133**; fixture do audit passou com ASan/UBSan (`detect_leaks=0`). Relatório `MIGRATION_SK2_17AL_SHADE_FLAG_AUDIT.md`; patch incremental `SK217AL.patch`. Sem comparação visual.

## SK2.17-AM — relógio ROUND e filhos `.eff` (2026-10-25)

Corrigido o rótulo anterior: tipo 12 é SHRINK, já tratado em SK2.17-P; ROUND é tipo 9. O audit Client mede 42 ROUND em 20 `.par`/7 skills e 10 filhos `.eff` em 8 `.par`/4 skills. DX9 acumula o ângulo uma vez por `FrameMove`, avança no máximo uma curva por chamada e roda o filho `CMPModelEff` (`arquivo/fontes/Engine/src/MPParticleSys.cpp:2917-2965`; `arquivo/fontes/Engine/include/MPParticleSys.h:116-129`). `TOP_HISTORICAL_ROUND_TIMELINE=1`, junto de `TOP_HISTORICAL_PARTICLE_EXTENDED=1` e desligado por padrão, agora registra o ângulo/cursor por atualização e atualiza o grupo de filho `.eff` uma vez por `EffectSystem::step(dt)` (`sources/Engine/include/top/HistoricalRoundTimeline.h`; `sources/Engine/src/top/EffectSystem.cpp:1060-1061,1190-1202,2078-2085,2177-2182,2374-2393`). O audit documenta ambas as features; no Client atual: 1.031 PAR, 1.227 EFF, 3.476 sistemas, 2.832 partes EFF, 26 lacunas, `drift=0`, falhas estruturais zero. Das 31 falhas estritas `.eff`, 30 são referenciadas diretamente; `fboomerff.eff` tem associação indireta por `fboomerff.par`/cena 919/skill 308; uso runtime segue desconhecido. Build Debug completo incluindo `top_gl`; CTest **135/135**. Sem comparação visual DX9/OpenGL. Relatório `MIGRATION_SK2_17AM_ROUND_TIMELINE.md`; patch incremental `SK217AM.patch`.

## SK2.17-AN — correção de classificação do filho EFF em ROUND (2026-10-25)

O audit ainda dizia que ROUND não tinha timeline para `.eff`, embora SK2.17-AM já tenha implementado `roundEffTimelines` sob opt-in. Corrigi a nota na fonte de cobertura e acrescentei uma asserção à fixture `FxCoverageAuditTests` para impedir que essa afirmação antiga volte. DX9 chama `CMPModelEff::FrameMove(dt)` em cada `_FrameMoveRound` (`arquivo/fontes/Engine/src/MPParticleSys.cpp:2926-2929`; implementação de cursor em `MPModelEff.cpp:500-540`); GL mantém a timeline por perfil e avança no passo real do sistema (`sources/Engine/src/top/EffectSystem.cpp:1105-1108,1200-1202,2374-2393`). A mudança não altera renderer/runtime. Audit Client: 1.031 PAR, 1.227 EFF, 3.476 sistemas, 2.832 partes, 26 lacunas, zero drift e falhas estruturais. Dos 31 `.eff` recusados em strict mode, 30 têm referências PAR diretas e um (`fboomerff.eff`) tem PAR homônimo ligado à cena 919/skill 308; runtime não comprovado. Configuração e build Debug incluindo `top_gl` passaram; CTest **135/135**. Sem comparação visual. Relatório `MIGRATION_SK2_17AN_ROUND_EFF_AUDIT_CORRECTION.md`; patch incremental `SK217AN.patch`.

## SK2.17-AO — controlador DUMMY experimental (2026-10-25)

Foi adicionado `TOP_HISTORICAL_DUMMY_CONTROLLER=1` para uma rota genérica de type 16 associada a matrizes de item. A auditoria posterior corrigiu a justificativa de uso: `sceneffectinfo` ID 152 aponta para `01040009.par`, mas a skill 24 Fireshield tem `sItemEffect1/2=0,0`; o valor 152 está em `sWhoped` e é passado a `PlayEnvSound` (`arquivo/fontes/Libraries/common/src/SkillRecord.cpp:420-424`; `arquivo/fontes/Client/src/EffDelay.cpp:27`). A nova trilha `fx_skill_item_channels.csv` não encontrou nenhum canal item apontando para type 16 no Client examinado. O modelo conter dummies 1/2 não prova uso do controlador. AO segue experimental, opt-in e sem consumidor de Client comprovado; não foi validado visualmente. O teste é sintético. Relatório e ressalva: `MIGRATION_SK2_17AO_DUMMY_CONTROLLER.md`.

## SK2.17-AP — auditoria de canais de item (2026-10-25)

O FX audit passou a gerar `fx_skill_item_channels.csv`: cruza colunas Item/ItemExtra de `skillinfo.txt` com IDs de `sceneffectinfo.txt` e tipos dos PAR decodificados. No Client atual encontrou 50 canais não nulos; nenhum tem candidato type 16 e Fireshield não tem canal de item. O relatório mantém `runtime_use_proven=no`, pois dados catalogados não comprovam execução na sessão. Resultado completo: `build/sk217ap-skill-item-audit/`. Build Debug dos alvos do audit e fixture sintética passaram; build completo/CTest desta etapa ainda pendente no momento deste registro. Relatório `MIGRATION_SK2_17AP_SKILL_ITEM_CHANNEL_AUDIT.md`.

## SK2.17-AQ — keyframes Item/ItemExtra (2026-10-25)

Adicionado o opt-in `TOP_HISTORICAL_ITEM_EFFECT_KEYFRAME` para separar e filtrar callbacks Item/ItemExtra por keyframe. O DX9 define `-1` início, `-2` fim e normaliza positivo para `0` (`arquivo/fontes/Client/src/STAttack.cpp:217-229,262-263`). A auditoria do Client achou quatro canais Item1 em `-1` (Cripple, Enfeeble, Headshot e Headshot Illusion), enquanto suas ações são `0`; todos usam ID 256/`01000016.par`. O porte anterior juntava todos os canais num cast; o novo escalonador cria callbacks separados e mantém o padrão legado desligado. Build Debug concluído e CTest 136/136 aprovado. Sem comparação visual; tempo do callback `0` ainda depende do relógio estimado de animação. Não alterou Frozen Arrow. Relatório: `MIGRATION_SK2_17AQ_ITEM_EFFECT_KEYFRAMES.md`.

A auditoria atual também encontrou 31 falhas do decodificador estrito EFF: 30 arquivos têm referências diretas nos PAR/sceneffectinfo e não devem ser tratados como descartáveis; o caso restante `fboomerff.eff` segue inconclusivo, não removido. Ver `build/sk217aq-fx-audit/fx_eff_usefulness.csv`.

## SK2.17-AR — proveniência do catálogo EFF DX9 (2026-10-25)

O FX audit passou a ler `CMPResManger::LoadTotalEffect` no original e a relatar por arquivo se um `.eff` no diretório principal é enumerado, carregado e indexado. Evidência DX9: `arquivo/fontes/Engine/src/MPResManger.cpp:1351-1405`. No Client, as 31 falhas estritas ficam no diretório enumerado; todas recebem `enumerated_loaded_indexed`, e o parser diagnóstico recupera as 31. Trinta também têm aresta PAR direta; `fboomerff.eff` segue associado ao PAR irmão/cena 919/skill 308. Isso prova catálogo/carga, não apresentação visual. Fixture cobre arquivo sem dependência direta e `.eff` em subdiretório. Build Debug completo e CTest passaram (136/136); o fixture também passou com ASan/UBSan (`detect_leaks=0`). Relatório `MIGRATION_SK2_17AR_EFF_DX9_CATALOG_AUDIT.md`; CSV/JSON em `build/sk217ar-eff-catalog-audit/`.

## SK2.17-AS — auditoria do flag billboard de WIND (2026-10-25)

O `top_par_audit` agora exporta em coluna `billboard` um byte que o parser já decodificava. O audit Client mediu 4 WIND em 3 PARs: dois `RectPlane` com billboard, um `RectPlane` sem billboard e um filho `fsummon1.eff`. O DX9 separa a matriz de câmera da rotação Z de `_FrameMoveWind` (`arquivo/fontes/Engine/src/MPParticleSys.cpp:3402-3415`); o renderer GL atual desenha o caminho type 7 como `ribbon` (`sources/Engine/src/top/EffectSystem.cpp:2706-2714`). Nenhuma mudança de runtime nesta etapa. Teste `ui05sk23_all_par_audit` passou; audit leu 1.031/1.031 PAR e 3.476 sistemas. Build completo, CTest completo e comparação visual não executados. Relatório `MIGRATION_SK2_17AS_WIND_BILLBOARD_AUDIT.md`; resultado `build/sk217as-wind-flag-audit/`.

## SK2.17-AT — geometria WIND billboard (2026-10-25)

Adicionado `TOP_HISTORICAL_WIND_BILLBOARD_GEOMETRY=1`, desligado por padrão. Restrito aos type 7 `RectPlane` com `billboard=1`, usa a rotação `velocity * idadeDoSlot` num quad da base de câmera do OpenGL; preserva o tamanho convertido existente e a textura/blend. DX9: `_CreateWind` inicializa velocidade angular e `_FrameMoveWind` acumula Z e monta a matriz (`arquivo/fontes/Engine/src/MPParticleSys.cpp:3270,3409-3442`). Medição AS: dois sistemas Client se encaixam no escopo; o type 7 não-billboard e o filho `.eff` ficam intocados. Build Debug completo incluindo `top_gl`; CTest **136/136**; integração sob ASan/UBSan passou (`detect_leaks=0`). Sem comparação visual. Relatório `MIGRATION_SK2_17AT_WIND_BILLBOARD_ROTATION.md`; patch `SK217AT.patch`.

## SK2.17-AU — WIND `RectPlane` no plano do mundo (2026-10-25)

Adicionado `TOP_HISTORICAL_WIND_WORLD_GEOMETRY=1`, desligado por padrão, para o único type 7 `RectPlane` não-billboard medido no Client. DX9 acumula o ângulo Z por `m_vVel.z * dt` (`arquivo/fontes/Engine/src/MPParticleSys.cpp:3270,3402-3415`); o GL traduz o plano XY/Z-up para X/Z/Y-up e desenha a quad nativa girada no plano X/Z (`sources/Engine/src/top/EffectSystem.cpp:387-400,2730-2735`). Integração compara ribbon e geometria de mundo; audit classifica somente esse conteúdo. Build Debug completo com `top_gl`; CTest **136/136**. Audit Client: 1.031 PAR, 1.227 EFF, zero falhas estruturais e zero deriva; 31 falhas estritas são recuperadas pelo modo diagnóstico, sem base para descartá-las. Integração ASan/UBSan passou (`detect_leaks=0`); comparação visual não executada; não há afirmação de fidelidade validada. Skill rastreada: 283. Relatório `MIGRATION_SK2_17AU_WIND_WORLD_RECTPLANE.md`; patch incremental `SK217AU.patch`.

## SK2.17-AV — timeline SHRINK com timer e reciclagem (2026-10-25)

O audit Client mede 42 type 12 SHRINK em 32 PARs/53 skills. DX9 `_CreateShrink` incrementa `_fCurTime` por tentativa de criação de slot morto, zera o timer no nascimento e sorteia posição/vida (`arquivo/fontes/Engine/src/MPParticleSys.cpp:2430-2475`); `_FrameMoveShrink` percorre slots em ordem, avança a curva uma vez por update, move os vivos e recicla os que morreram na caixa (`:2497-2528,2565-2573`; `MPParticleSys.h:116-129`). `TOP_HISTORICAL_SHRINK_TIMELINE=1`, novo e desligado por padrão, persiste timer, slots, quadros e geração por perfil usando `EffectSystem::step(dt)`; deterministic seed aproxima o RNG global. O decoder estrito continua rejeitando 31 EFF, todos recuperados pelo parser diagnóstico; 30 têm referências diretas e `fboomerff.eff` tem PAR homônimo/cena 919/skill 308, sem prova de apresentação. Build Debug completo com `top_gl`; CTest **136/136**; unitário e integração passaram sob ASan/UBSan (`detect_leaks=0`). Sem comparação visual; SHRINK segue aproximado. Relatório `MIGRATION_SK2_17AV_SHRINK_RECYCLE_TIMELINE.md`; patch incremental `SK217AV.patch`.

## SK2.17-AW — evidência de donos para CMPStrip direto (2026-10-25)

Corrigido o `top_direct_strip_anchor_audit`: a classificação anterior `player_owner_path_present` inferia presença do dono de cada canal a partir de caminhos genéricos no DX9/GL. O original exige `_pItem` ou `_pCha` em `CMPStrip::Play` (`arquivo/fontes/Engine/src/MPModelEff.cpp:1251-1272`); `MPEffectCtrl` tem callsites condicionais para `SetStripItem` (`arquivo/fontes/Engine/src/MPEffectCtrl.cpp:309-315,415-420`). A auditoria AW também omitiu `Client/src/EffectObj.cpp`, onde existem callsites condicionais para personagem e item (`EffectObj.cpp:1048-1060`); SK2.17-BB corrige a conclusão de ausência do setter de personagem. O GL tem provedores apenas para corpo/item do player (`sources/Engine/src/top/EffectSystem.cpp:1633-1651`; `ActorSkin.cpp:1408-1468`).

Audit Client refeito: 1.031 `.par`, 6 trailers diretos/6 arquivos, 16 IDs únicos, zero falhas de decodificação e um arquivo sem dependência. Agora classifica separadamente canal actor sem setter, item com callsite mas runtime não comprovado, canal de superfície sem owner provado e dependência ausente. Relata os callsites e provedores como evidência estática; não altera renderer nem declara uso visual. Os alvos de audit compilaram em Debug; CTest completo passou **136/136**; fixture focal passou sob ASan/UBSan (`detect_leaks=0`; LSan não executado). Sem comparação visual. Relatório `MIGRATION_SK2_17AW_DIRECT_STRIP_OWNER_EVIDENCE.md`; saída `build/sk217aw-direct-strip-audit/`; patch incremental `SK217AW.patch`.

## SK2.17-AX — catalogação scene-effect no audit CMPStrip (2026-10-25)

O `01030010.par` antes sem dependência de skill tem entrada ID 217 (“Sea Wave”) em `Client/scripts/table/sceneffectinfo.txt`. DX9 carrega essa tabela (`arquivo/fontes/Client/src/GameAppInterface.cpp:1359`) e tem o caminho `AddSceneEffect(id) → CEffectObj::Create(id)` (`SceneCreateNode.cpp:325-341`; `EffectObj.cpp:669-710`), mas nenhuma execução do ID 217 foi comprovada. O audit de trailers agora cruza `sceneffectinfo`: todos os seis CMPStrip estão catalogados; seis IDs/nomes aparecem na nova coluna, enquanto `01030010.par` segue sem dependência de skill. CTest completo **136/136**, fixture focal e ASan/UBSan passaram (`detect_leaks=0`; LSan não executado). Somente auditoria, sem alteração de renderer e sem validação visual. Relatório `MIGRATION_SK2_17AX_SCENE_EFFECT_CATALOG_AUDIT.md`; saída `build/sk217ax-direct-strip-audit/`; patch incremental `SK217AX.patch`.

## SK2.17-AY — inventário de efeitos estáticos nos mapas (2026-10-25)

Adicionado `top_scene_effect_map_audit`, leitura somente de `.obj` versão 600 e cruzamento com `sceneffectinfo.txt`. O Client medido tem 47 mapas válidos, 10.770 colocações tipo 1 e 21 IDs; 10.760 colocações catalogadas. Os 10 sem catálogo são ID 21 (6) e ID 22 (4). DX9 lê o `SSceneObjInfo`, cria `CEffectObj` e aplica posição/altura/yaw (`arquivo/fontes/Client/src/SceneObjFile.h:15-31`; `SceneObjFile.cpp:477-493`; `GameAppMsg.cpp:128-136`; `SceneCreateNode.cpp:325-338`). Atualiza efeitos válidos por tick e invalida na renderização quando o PartCtrl para; IDs 1–99 são excluídos do timeout e os efeitos de cena são limpos na troca de mapa (`SceneFrameMove.cpp:119-130`; `EffectObj.cpp:1088-1138,1142-1207,1417-1439`; `Scene.cpp:1771-1779`). O GL ignora os registros tipo 1 (`sources/Engine/src/top/SceneWorld.cpp:465-470`). Auditoria não modifica runtime; falta verificar slots sem definição no catálogo e semântica de Play/SetPlayType antes de implementar a instanciação. Build Debug completo com `top_gl` e CTest **137/137** passaram; audit Client foi executado; fixture focal ASan/UBSan passou (`detect_leaks=0`); comparação visual não se aplica por não haver mudança gráfica. As 31 falhas EFF estritas foram verificadas separadamente: todas recuperadas no modo diagnóstico e catalogadas/carregadas pelo DX9; 30 referenciadas diretamente por PAR. `fboomerff.eff` não tem referência direta, mas tem PAR irmão, sceneffectinfo 919 e skill 308, sem prova de execução. Relatório `MIGRATION_SK2_17AY_SCENE_EFFECT_MAP_AUDIT.md`; CSV/JSON em `build/sk217ay-scene-effect-map-audit/`; patch `SK217AY.patch`.

## SK2.17-AZ — cobertura dos controllers PAR dos efeitos de mapa (2026-10-25)

O `top_scene_effect_map_audit` agora cruza IDs de mapa com `LegacyParAudit` e grava `scene_effect_map_controllers.csv`. Os 19 IDs catalogados de mapa resolvem para 19 PARs e todos decodificam; contêm 33 sistemas dos tipos 1, 2, 3, 4 e 6. Tipo 6 responde por 7.117 das colocações catalogadas, principalmente ID 1/`wave1.par` (6.771, `bonew5.eff`). Os 10 registros ID 21/22 continuam sem linha explícita em `sceneffectinfo`, sem classificação de uso. DX9 cria, emite com `Play(0)`/`SetPlayType(0)`, atualiza por tick, encerra por `IsPlaying` e limpa efeitos de cena na troca de mapa (`EffectObj.cpp:1293-1351,1142-1207`; `MPParticleCtrl.cpp:392-405`; `SceneFrameMove.cpp:119-130`; `Scene.cpp:1771-1779`). GL sabe resolver e carregar PAR por ID via `SceneEffectCatalog`/`resourceFor` (`EffectSystem.cpp:413-425,488-505`), mas `SceneWorld` pula `kind != 0` (`SceneWorld.cpp:465-470`), então não instancia colocações do `.obj`. Fixture focal de mapa/PAR passou; audit atualizado confirmou 19/19 PARs decodificados, 33 sistemas e zero registros de mapa malformados. Audit PAR atual: 1.031 decodificados/3.476 sistemas. FX audit atual: 31 falhas EFF estritas, todas recuperadas em diagnóstico; 30 com PAR direto, e nenhum nome EFF das colocações de mapa cruza esse grupo de 31. Relatório `MIGRATION_SK2_17AZ_SCENE_EFFECT_CONTROLLER_AUDIT.md`; CSVs/JSON em `build/sk217az-scene-effect-controller-audit/`, `build/scene-effect-par-audit-current/` e `build/fx-coverage-current/`. Sem mudança runtime ou validação visual.

## SK2.17-BA — controladores de efeitos colocados no mapa (2026-10-25)

Adicionado `TOP_HISTORICAL_MAP_SCENE_EFFECTS=1`, desligado por padrão, para consumir registros tipo 1 dos `.obj`, resolver o ID no `sceneffectinfo.txt` e instanciar o PAR catalogado como efeito persistente durante o streaming da área. O caminho traduz yaw DX9 Z-up para rotação horizontal GL X/Z em mundo Y-up e preserva posição/altura; não cria partículas substitutas. DX9: leitura do registro e unidade (`arquivo/fontes/Client/src/SceneObjFile.h:15-31`; `SceneObjFile.cpp:477-493`), criação (`GameAppMsg.cpp:128-136`; `SceneCreateNode.cpp:325-338`), emissão/ciclo (`EffectObj.cpp:1334-1351`; `MPParticleCtrl.cpp:392-405`) e atualização (`SceneFrameMove.cpp:119-130`). Antes, o GL descartava `kind != 0` (`sources/Engine/src/top/SceneWorld.cpp`, ramo de registros agora substituído) apesar de carregar o catálogo (`EffectSystem.cpp:413-425`).

Reauditoria `.eff`: 31 recusas estritas, zero falhas estruturais; todas as 31 são recuperáveis no caminho diagnóstico. 30 têm referências PAR diretas. `fboomerff.eff` é o caso sem aresta direta, mas tem PAR irmão catalogado (ID 919/skill 308) e o DX9 enumera/carrega o diretório (`arquivo/fontes/Engine/src/MPResManger.cpp:1351-1405`); execução não provada, portanto não foi descartado nem alterado o decoder. Audit atualizado em `build/sk217ba-fx-audit/`.

A auditoria de mapa registra 47 mapas válidos, 10.770 colocações tipo 1, 21 IDs; 10.760 colocações têm catálogo. IDs 21/22 (10 posições) seguem sem definição explícita. O audit de controladores decodificou 19 PARs/33 sistemas. Build Debug completo incluindo `top_gl`; CTest **138/138**; integração focal e fixture UI05X3 sob ASan/UBSan **2/2** (`detect_leaks=0`). Sem comparação visual DX9/OpenGL: a opção segue experimental e não é declarada visualmente equivalente. Relatório `MIGRATION_SK2_17BA_MAP_SCENE_EFFECTS.md`; patch incremental `SK217BA.patch`.

## SK2.17-BB — correção do audit de owners CMPStrip (2026-10-25)

A reauditoria do FX ranking continua com type 6 no topo (1.041 sistemas/242 skills), mas suas linhas não significam ausência geral de renderização: várias etapas `.eff` já têm timelines. Ao rever o dono de CMPStrip, encontrei um falso-negativo no `top_direct_strip_anchor_audit`: ele não lia `arquivo/fontes/Client/src/EffectObj.cpp`, onde `CEffectObj::SetEffect` chama `SetStripCharacter` para `NODE_CHA` e `SetStripItem` para `NODE_ITEM` se houver strips (`EffectObj.cpp:1048-1060`; encaminhamento em `MPParticleCtrl.cpp:326-340`). Adicionei `--dx9-client-effect-obj`, colunas de linhas por fonte no CSV e evidência separada no JSON. No Client atual: seis trailers/seis arquivos, zero erros; os três canais actor mostram o callsite em `:1055`; item tem callsites em MPEffectCtrl e `EffectObj.cpp:1060`. Conclusão antiga `actor_setter_callsite_missing` corrigida para `actor_setter_callsite_present_runtime_unproven` nos relatórios V/AW. A diferença de owner persiste: o GL consulta dummies do corpo/item do jogador (`EffectSystem.cpp:1676-1686`; `ActorSkin.cpp:1408-1468`), sem providers genéricos de outras entidades comprovados. FX audit Client atual: 1.031 PAR, 1.227 EFF, 3.476 sistemas, 2.832 partes, 26 lacunas/307 skills, deriva 0/falhas estruturais 0. `.eff`: 31 falhas estritas, todas recuperáveis no diagnóstico; 30 têm referência direta, e `fboomerff.eff` permanece indeterminado apesar do PAR irmão ID 919/skill 308 e carga/indexação DX9. Nenhum arquivo removido. Alvos do audit e `top_gl` compilados em Debug; CTest completo **138/138**; fixture focal ASan/UBSan **1/1** (`detect_leaks=0`, `UBSAN_OPTIONS=halt_on_error=1`). O patch passou checagem/aplicação na cópia isolada e os sete arquivos foram comparados byte a byte; saídas em `build/sk217bb-direct-strip-audit-final/` e `build/sk217bb-fx-audit-final/`. Sem validação visual. Relatório `MIGRATION_SK2_17BB_DIRECT_STRIP_CALLSITE_AUDIT_FIX.md`; patch incremental `SK217BB.patch`.

## SK2.17-BC — dispatch de hitEffect por tipo (2026-10-25)

Auditoria comparativa: o DX9 despacha `SendResMessage` para FIRE (tipo 2) e RANGE2 (15); MODEL (5) e STRIP (6) têm o campo serializado sem call site correspondente nesta fonte. O porte já contém caminhos condicionais para FIRE/RANGE2. O auditor agora produz `fx_hit_effect_coverage.csv` e retira a lacuna genérica do ranking apenas se enum, dispatch DX9 e caminhos GL forem verificados. Client: 66 campos em 46 PARs/9 skills (FIRE 5, MODEL 21, STRIP 5, RANGE2 35); quatro call sites DX9. Resultado: 1.031 PAR, 1.227 EFF, 3.476 sistemas, 2.832 partes; 25 gaps, zero drift/falhas estruturais. As 31 recusas estritas EFF continuam recuperáveis em modo diagnóstico; 30 têm referência direta e `fboomerff.eff` permanece sem uso de runtime provado. Build Debug incluindo `top_gl`; CTest **138/138**; fixture audit focal ASan/UBSan **1/1** (`detect_leaks=0`). Sem comparação visual; mudança só no auditor. Relatório `MIGRATION_SK2_17BC_HIT_EFFECT_DISPATCH_AUDIT.md`; patch incremental `SK217BC.patch`.

## SK2.17-BD — capacidade e fim de CMPStrip (2026-10-25)

O ranking de controladores mediu seis direct strips em seis PARs, com vínculos estáticos a 16 skill IDs. O DX9 guarda os pares mesmo depois do fade; para os caminhos actor/item de jogador cobertos pelo GL, a configuração é não-loopada: CMPStrip constrói false, SetStripCharacter não altera o loop e SetStripItem recebe false por default. O DX9 para no limite maxLength e deixa de desenhar; o GL anterior removia pares expirados e mantinha uma janela móvel. TOP_HISTORICAL_CMPSTRIP_LIFECYCLE=1 agora retém esses pares e para a emissão no limite, com a regra de crescimento em lotes de 100 também coberta no helper. A opção fica desligada por padrão. Teste focal CMPStrip 1/1, build Debug completo incluindo top_gl, CTest 138/138 e fixture ASan/UBSan 1/1 (detect_leaks=0). Sem comparação visual. Owners de NPC/monstro, vínculo de loop true MPEffectCtrl e execução dos registros permanecem não provados. Relatório MIGRATION_SK2_17BD_CMPSTRIP_CAPACITY_LIFECYCLE.md; patch incremental SK217BD.patch.

## SK2.17-BE — amostragem `Randf` de SNOW/FIRE (2026-10-25)

O FX audit mediu `feature:snow_fire_step` em 588 sistemas/266 PARs/123 skills estáticos. O original implementa `Randf(float)` por `rand()%N` e frações em grade de 0,001 (`arquivo/fontes/Engine/include/MPParticleSys.h:6-44`); o porte opt-in usava amostragem float contínua. Adicionei `historical_particle_random::dx9Randf` para traduzir o mapeamento/modulo sobre o seed determinístico existente, aplicado a vida, spawn e velocidades SNOW; `rand()%2` também é traduzido nos sinais de aceleração. `TOP_HISTORICAL_PARTICLE_RANDF=1` isola a mudança e fica desligado por padrão, preservando as duas integrações existentes sem essa variável. O caso DX9 vida 2/divisor 4 resulta em 0,5–0,999, verificado em fixture; a sequência global `rand()` e a intenção possivelmente esperada pelo autor permanecem desconhecidas. Build Debug incluindo `top_gl` e CTest 138/138 passaram após o gate; teste focal ASan/UBSan passou. Auditoria Client: 1.031 PAR, 1.227 EFF, 3.476 sistemas, 25 gaps, 307 skills estáticas, zero drift/falhas estruturais. Sem comparação visual. Relatório `MIGRATION_SK2_17BE_SNOW_FIRE_RANDF_MAPPING.md`; patch incremental `SK217BE.patch`.

## SK2.17-BF — auditoria do RNG global e classificação de `.eff` (2026-10-25)

O scanner estático separa sites DX9 por Client/Engine/Libraries e identifica declarações MaxSDK e ferramenta Log Viewer fora das contagens de runtime candidato. Encontrou sites `rand()` no Client (41), Engine (13) e Libraries (9), oito `srand()` no Client e dois em bibliotecas (Lua/EncLib); OpenGL Engine tem zero chamadas `rand`/`srand` e 23 referências lexicais aos helpers determinísticos. São sites de código, não draws executados. `GameLoading.cpp:63-72` reseeda e consome `rand()` em `WM_PAINT`; Lua `math.randomseed` e EncLib `SetEncKey` também reseedam o estado C (`arquivo/fontes/Libraries/CaLua/Lua/lib/lmathlib.c:170-199`; `EncLib/src/enclib.cpp:59-89`). A ordem/alcance por sessão permanece desconhecida, então não há mudança runtime nesta etapa e o mapeamento `Randf` segue opt-in. As 31 recusas strict `.eff` do FX audit decodificam no modo diagnóstico e têm evidência de enumeração/indexação DX9; 30 referenciam PAR diretamente, e `fboomerff.eff` tem PAR irmão/catalogação, sem uso runtime provado. Nenhum foi descartado. Fixture do scanner 1/1; build Debug incluindo `top_gl` e CTest 138/138; auditoria DX9/GL executada. Sem comparação visual. Relatório `MIGRATION_SK2_17BF_DX9_GLOBAL_RNG_AUDIT.md`; patch incremental `SK217BF.patch`.

## SK2.17-BG — mapeamento Randf do BLAST (2026-10-25)

A lacuna `type:3` mede 423 sistemas/118 PARs/101 skills estáticos. DX9 `_CreateBlast` usa `Randf` para velocidade, range e vida, além de `Rand(2)` nos sinais horizontais (`arquivo/fontes/Engine/src/MPParticleSys.cpp:3844-3877`; helper `MPParticleSys.h:6-44`); GL usava float contínuo em `HistoricalParticleMotion.h` e `HistoricalParticlePool.h`. Estendi `TOP_HISTORICAL_PARTICLE_RANDF=1` (desligado por padrão) a esses draws BLAST, preservando o caminho atual sem switch e sem alegar reproduzir a ordem global do CRT. Fixture focal cobre intervalo de vida e grade 0,001; configuração Debug/build completo incluindo `top_gl` e CTest 138/138 passaram; ASan/UBSan focal passou (`detect_leaks=0`, `UBSAN_OPTIONS=halt_on_error=1`). FX audit atualizado: 1.031 PARs/1.227 EFFs/3.476 sistemas, 25 gaps/307 skill IDs, drift 0/falhas estruturais 0; as 31 recusas strict EFF são diagnosticamente decodificáveis e nenhuma foi descartada. Sem comparação visual. Relatório `MIGRATION_SK2_17BG_BLAST_RANDF.md`; patch `SK217BG.patch`.

## SK2.17-BH — triagem atual do ranking STRIP e anchors (2026-10-25)

FX audit atual: 1.031 PAR/1.227 EFF/3.476 sistemas/2.832 partes, 25 gaps/307 skill IDs, drift 0/falhas 0. `type:6` tem 1.041 sistemas/242 skills e agrega subfeatures: 875 sistemas `.eff` já têm timelines opt-in R/S; direct CMPStrip são seis trailers catalogados em seis PARs, zero falhas de decode e 16 IDs de skill estáticos; um trailer sem linha de dependência skill. DX9 `EffectObj::SetEffect` liga strips a `NODE_CHA`/`NODE_ITEM` (`arquivo/fontes/Client/src/EffectObj.cpp:1048-1060`); GL processa matrizes de Cast do jogador (`EffectSystem.cpp:1676-1686`). Os vínculos estáticos indicam skill 266 para xzsb1/2, skill 238 “Colossus Smash” para `mosterlight`, e item para `swordlight02`; não provam execução nem owner em runtime. `MPEffectCtrl` também chama `SetStripItem(...,true)` (`arquivo/fontes/Engine/src/MPEffectCtrl.cpp:313,419`), caminho cujo alcance/loop real no GL não está provado. Sem nova alteração C++: falta trace DX9 para decidir sobre owners não-player/impacto de superfície. A triagem EFF atual mantém as 31 recusas estritas como úteis/catálogo e enumeração/indexação DX9; 30 têm PAR direto, `fboomerff.eff` segue runtime não provado. Relatório `MIGRATION_SK2_17BH_STRIP_CONTROLLER_GAP_TRIAGE.md`; CSV/JSON `build/sk217bg-direct-strip-audit/` e `build/sk217bg-fx-audit/`.

## SK2.17-BI — Randf no nascimento RANGE2 (2026-10-25)

RANGE2 type 15 está medido no FX audit com 35 sistemas/19 PARs/7 IDs estáticos de skill; isso prova cobertura de dados, não execução. DX9 `_CreateRange2` usa `Randf(_fRange[0/1])` e mantém Z fixo (`arquivo/fontes/Engine/src/MPParticleSys.cpp:2142-2155`; helper em `MPParticleSys.h:6-37`). Em `HistoricalRange2Collision.h`, os dois caminhos de nascimento/amostra usavam `range*random01`. Adicionei a seleção `TOP_HISTORICAL_PARTICLE_RANDF=1` usando `deterministicRandf` nesses dois caminhos; ausente/0 preserva as expressões anteriores. O GL continua convertendo XYZ DX9 para X/Z/Y somente na fronteira de renderização (`EffectSystem.cpp:2414-2420`). Fixture nova cobre caminho desligado, amostra determinística ligada, limites e quantização de 0,001. Configuração Debug, compilação completa incluindo `top_gl`, CTest 139/139 e fixture ASan/UBSan terminaram com código 0. O patch passou aplicação/reversão e comparação dos arquivos base em cópia isolada. O fluxo determinístico do GL não reproduz a ordem do `rand()` global MSVC; sem comparação visual, paridade não validada. Relatório `MIGRATION_SK2_17BI_RANGE2_RANDF.md`; patch incremental `SK217BI.patch`.

## SK2.17-BJ — resultado do factory DX9 para IDs de efeitos nos mapas (2026-10-25)

Estendi `top_scene_effect_map_audit` para verificar por ID a cadeia `GameAppMsg → AddSceneEffect → CMagicEff::Create → GetMagicInfo/CMagicSet → sceneffectinfo.txt` e registrar estado/evidência do factory. Client atual: 47 mapas válidos, 10.770 colocações, 10.760 catalogadas; os 10 registros restantes são IDs 21 (6) e 22 (4), sem entrada de catálogo. A cadeia de código DX9 foi encontrada, mas a ausência da linha não prova factory nulo: `GetRawDataInfo` devolve slot pré-alocado para IDs válidos (`MPResData.h:164-173`). Por isso, ambos ficam `dx9_catalog_record_absent_factory_result_unproven`; o GL continua ignorando IDs sem definição PAR, sem inventar recurso. Auditoria não altera runtime. Alvos do audit/teste compilaram em Debug; CTest completo 139/139; fixture ASan/UBSan terminou com código 0. Saída `build/sk217bj-map-factory-audit/`; relatório `MIGRATION_SK2_17BJ_MAP_FACTORY_EVIDENCE.md`; patch incremental `SK217BJ.patch`. A triagem EFF continua: 31 recusas estritas recuperáveis no modo diagnóstico; 30 com referência PAR direta e `fboomerff.eff` com uso runtime desconhecido.

## SK2.17-BK — reexecução do audit de CMPStrip com fonte de owner explícita (2026-10-25)

O artefato AX ainda dizia `dx9_character_setter_callsite_observed=false`, contrariando `CEffectObj::SetEffect` (`arquivo/fontes/Client/src/EffectObj.cpp:1048-1060`). Recompilei `top_direct_strip_anchor_audit` e passei explicitamente `--dx9-client-effect-obj`. A saída nova registra `SetStripCharacter` na linha 1055 e `dx9_character_setter_callsite_observed=true`. Client: 1.031 PARs, seis trailers/seis arquivos, 16 IDs estáticos, seis entradas scene-effect, zero falhas de decode, um arquivo sem dependency row. xzsb1/2 e mosterlight têm canal actor; swordlight02 item; 01000012 item/impact_ground; 01030010 está catalogado como scene effect 217. São vínculos estáticos, não execução comprovada. GL limita direct strips a Cast do player e provedores body/item do player (`EffectSystem.cpp:1676-1686`; `ActorSkin.cpp:1408-1468`); owners não-player continuam sem rota comprovada, portanto sem mudança de renderer. Build focal e CTest 1/1 passaram. Saída atual `build/sk217bk-direct-strip-audit/`; relatório `MIGRATION_SK2_17BK_DIRECT_STRIP_AUDIT_REFRESH.md`; patch incremental `SK217BK.patch`. Sem comparação visual.

## SK2.17-BL — auditoria de associação a CMagicCtrl (2026-10-25)

O `top_direct_strip_anchor_audit` agora cruza os trailers CMPStrip com os nomes de PAR da tabela `MagicSingleinfo`, usando a mesma tabela que `CEff_ParamSet` lê no DX9. O Client atual tem seis trailers em seis PARs, todos no catálogo `sceneffectinfo`; nenhum tem correspondência na coluna de PAR filho de `MagicSingleinfo`. O DX9 contém `CMagicCtrl::Create` com `SetStripItem(..., true)` (`MPEffectCtrl.cpp:401-420`), mas os seis recursos medidos não estão ligados estaticamente a esse caminho. O GL mantém `false` no caminho direto de jogador, coerente com a chamada DX9 comum e o default do setter. Sem alteração runtime. Configuração e build Debug completos, incluindo `top_gl`, terminaram com exit 0; CTest completo 139/139 passou. Saída `build/sk217bl-direct-strip-audit/`; relatório `MIGRATION_SK2_17BL_DIRECT_STRIP_MAGIC_OWNER_AUDIT.md`. As 31 recusas estritas históricas de `.eff` foram recuperadas no modo diagnóstico e têm evidência de catálogo/referência, não devem ser marcadas como mortas; a auditoria atual mais recente registrou zero falhas estruturais. Uso runtime individual e comparação visual DX9/GL continuam desconhecidos.

## SK2.17-BM — distribuição Randf de nascimento WIND (2026-10-25)

FX coverage audit Client atual mede WIND tipo 7 em 4 sistemas/3 PARs/2 IDs estáticos de skill, ainda marcado approximate e não visualmente verificado. O DX9 chama `Randf` nos três eixos no `_CreateWind` (`arquivo/fontes/Engine/src/MPParticleSys.cpp:3254-3262`; helper `MPParticleSys.h:26-37`); o GL usava spawn contínuo comum a WIND/RANGE (`HistoricalParticleExtended.h:85-98,127-129`). Agora `TOP_HISTORICAL_PARTICLE_RANDF=1`, desligado por padrão, seleciona `deterministicRandf` somente no type 7 (`HistoricalParticleExtended.h:85-98`); RANGE permanece intocado. A tradução reproduz distribuição/quantização sobre seed/lane, não a sequência do `rand()` global do DX9. FX audit: 1.031 PAR/1.227 EFF/3.476 sistemas/2.832 partes, 25 gaps/307 skill IDs, drift 0/falhas estruturais 0; 31 recusas EFF strict recuperáveis em diagnóstico, 30 com PAR direto e um sem referência direta mas com evidência de catálogo/par homônimo, sem runtime provado. Build Debug incluindo `top_gl`; CTest **139/139**. Fixture focal também passou sob ASan/UBSan (`detect_leaks=0`, `UBSAN_OPTIONS=halt_on_error=1`). Sem comparação visual; WIND continua aproximado. Relatório `MIGRATION_SK2_17BM_WIND_RANDF_SPAWN.md`; patch incremental `SK217BM.patch`.


## SK2.17-BN — revisão individual de utilidade dos `.eff` strict-only (2026-10-25)

Reexecutei `top_fx_coverage_audit` na pasta `Client/`: 1.031 PAR/1.227 EFF/3.476 sistemas/2.832 partes, 25 lacunas/307 skill IDs estáticos, drift 0 e falhas estruturais 0. As 31 recusas strict-only decodificam no modo diagnóstico com parâmetros crus: 30 têm 32 arestas PAR diretas; `fboomerff.eff` não tem aresta, mas tem PAR homônimo catalogado (ID 919/skill estática 308). DX9 enumera `*.eff`, tenta carregar e indexa o nome até se `LoadEffectFromFile` falhar (`MPResManger.cpp:1351-1405`); isso não comprova sucesso nem runtime. `I_Effect.cpp:517-520,662-669` serializa/lê floats de parâmetros de modelo sem validação de finitude. Não há base para descartar qualquer um dos 31; nenhum foi removido ou mudou decoder. Runtime de `fboomerff` e intenção/segurança de NaN/Inf permanecem desconhecidos; sem comparação visual. Inventário de cada arquivo em `MIGRATION_SK2_17BN_EFF_USEFULNESS_REFRESH.md`, CSV/JSON em `build/sk217cn-fx-audit/`. Etapa somente audit, sem mudança C++.

## SK2.17-BO — consumidor estático de `fboomerff.eff` (2026-10-25)

Rastreei as entradas conhecidas de `GetEffectID`: modelo vindo de PAR (`MPParticleSys.cpp:292-301`) e modelos de tabelas `CEffectScript`/`EFF_Param` (`MPEffectCtrl.cpp:33-49,245-251,353-367`). Busca literal nos arquivos binários/textuais de código/tabelas encontrou `fboomerff` só em `sceneffectinfo.txt:551`, que aponta para `fboomerff.par`; `dependencies.csv:491` liga esse PAR à skill 308/canal `impact_target`. `top_par_audit` atual decodifica o PAR em 3 sistemas type 15, todos `RectPlane`, com texturas `fire_02`, `smoke`, `poisonshadow` e sem referência EFF. O audit FX registra zero arestas PAR→`fboomerff.eff`. O GL resolve modelos `.eff` sob demanda de PAR/tabelas (`EffectSystem.cpp:486-500,515-570,579-597,600-621`), portanto não há consumidor estático conhecido que exija decodificar esse arquivo no caminho GL. **Não** removi o asset nem afirmo ausência absoluta de chamadas DX9 dinâmicas: o DX9 enumera/carrega globalmente (`MPResManger.cpp:1351-1405`). Etapa audit-only; relatório `MIGRATION_SK2_17BO_FBOOMERFF_CONSUMER_TRACE.md`. Runtime/captura visual ainda não executados.

## SK2.17-BP — timeline compartilhada de `.eff` para SNOW/FIRE (2026-10-25)

O `top_par_audit` agora preserva o byte `mediay` de PAR v15. No Client atual, nenhum dos 3.476 sistemas tem `mediay=true`. A auditoria de dados encontrou 238 sistemas SNOW/FIRE com `.eff` filho (102 SNOW, 136 FIRE) em 99 PARs, ligados a 70 IDs estáticos no FX audit. O DX9, quando `mediay=false`, toca um único `CMPModelEff` e chama seu `FrameMove(dt)` uma vez por update de SNOW/FIRE (`arquivo/fontes/Engine/src/MPParticleSys.cpp:751-756,3570-3576,3743-3749`); o GL usava fase analítica baseada na idade da partícula. Adicionei o relógio compartilhado por perfil atrás de `TOP_HISTORICAL_SNOW_FIRE_EFF_TIMELINE=1`; o modo `mediay=true` por slot continua sem suporte e não aparece nos assets examinados. FX audit atual: 25 lacunas/307 IDs estáticos, zero drift e falhas estruturais. Build Debug completo incluindo `top_gl`, CTest **140/140**, fixture ASan/UBSan e FX audit 3/3 passaram. Sem comparação visual. Relatório `MIGRATION_SK2_17BP_SNOW_FIRE_SHARED_EFF_TIMELINE.md`; patch incremental `SK217BP.patch`.

## SK2.17-BQ — auditoria da ordem RNG SNOW/FIRE (2026-10-25)

Comparei a ordem de `Rand`/`Randf` nos nascimentos e updates DX9 com os lanes/seed determinísticos do GL. O Client auditado contém 1.070 SNOW/FIRE (566/504), zero `model_range`; a vida e o movimento usam derivações de seed distintas, portanto lane igual não prova draw repetido. Retirei a tentativa de runtime por não haver evidência de que offsets isolados reproduzam melhor o original. Reaudit `particle_rng_audit.py`: 41 rand Client, 13 Engine, 9 Libraries; o porte GL tem zero rand/srand e 27 referências de helpers determinísticos. Scanner não prova reachability/ordem. Build Debug e CTest completo executados após retirar a tentativa; sem comparação visual. Relatório `MIGRATION_SK2_17BQ_SNOW_FIRE_RNG_ORDER_AUDIT.md`; saída `build/sk217bq-rng-order-audit/`.

## SK2.17-BR — midpoint do controlador MODEL (2026-10-25)

DX9 `_FrameMoveModel` grava `_vPos + range/2` no slot único (`arquivo/fontes/Engine/src/MPParticleSys.cpp:3097-3101`); o GL zerava o corner de type 5 no ramo `singleModel`. `TOP_HISTORICAL_MODEL_POSITION=1` (off por padrão) adiciona o midpoint, mapeando DX9 X/Y/Z para GL X/Z/Y; type 6 STRIP permanece no ramo anterior. O FX audit deriva `model_midpoint_position` de type 5 com range não zero: Client atual 161 sistemas/102 PARs/9 IDs estáticos; 777 type 5 no total. Build Debug completo com `top_gl`, CTest 140/140, fixture integrada e audit atualizado passaram; fixture focal também passou em ASan/UBSan (`detect_leaks=0`). O FX audit registra 26 gaps/307 skill IDs, drift 0/falhas 0. 31 EFF strict-only foram novamente medidos: 31 recuperáveis em diagnóstico, 30 com PAR direto, nenhum com uso runtime provado; nenhum removido. Sem comparação visual. Relatório `MIGRATION_SK2_17BR_MODEL_MIDPOINT_POSITION.md`; saída `build/sk217br-model-position-audit/`.

## SK2.17-BS — fim da retenção do controlador MODEL/STRIP (2026-10-25)

O DX9 `UpdateDelay` para MODEL/STRIP ao atingir `playTime` absoluto desde `Play()` (`arquivo/fontes/Engine/src/MPParticleSys.cpp:176-233,907-924`); `Stop/IsPlaying` e `CMagicEff` removem o dono (`MPParticleSys.cpp:1121-1185`; `MPParticleCtrl.cpp:441-466`; `Client/src/EffectObj.cpp:1202-1207`). O GL já suprimia desenho nesse tempo, mas retinha a instância até `delay+playTime+life` (`EffectSystem.cpp:1052-1056,2090-2099,1425-1427`). `TOP_HISTORICAL_MODEL_LIFETIME=1`, desligado por padrão, limita a expiração deste perfil a `max(delay,playTime)`; os prazos maiores de outros perfis continuam valendo. Client atual: 777 MODEL, 167 com delay, 299 com play finito positivo, 319 com delay ou play não zero. Fixture de PAR v6 integrou os modos opt-in/off; contagem combinada MODEL/STRIP em `fx_feature_coverage.csv`. Relatório `MIGRATION_SK2_17BS_MODEL_CONTROLLER_LIFETIME.md`; auditorias em `build/sk217bs-model-lifetime-audit/`. Sem comparação visual.

## SK2.17-BT — ordem do atraso na timeline MODEL (2026-10-25)

DX9 `_FrameMove` retorna em `IsDelay()` antes de `FrameUpdate` (`arquivo/fontes/Engine/src/MPParticleSys.cpp:1243-1257`); `Render/UpdateDelay` só incrementa o relógio depois (`:176-189,1260-1265`). O GL anteriormente aplicava uma fração do `dt` no update que cruzava `delayTime` (`sources/Engine/src/top/EffectSystem.cpp:1193-1197`). `TOP_HISTORICAL_MODEL_FRAME_STEP=1` agora pula esse update inteiro para o pai e `.eff` filho; o próximo consome o `dt` completo (`HistoricalModelTimeline.h`; `EffectSystem.cpp:1262-1274`). Client atual: 114 sistemas MODEL multi-quadro com delay, 50 PARs/39 IDs de skills; 103 incluem `.eff` filho. Testes focal unitário/integrado Debug e ASan/UBSan passaram. Configuração e build Debug completos, incluindo `top_gl`, exit 0; CTest completo **141/141**. Relatório `MIGRATION_SK2_17BT_MODEL_DELAY_FRAME_STEP.md`; audit `build/sk217bt-model-delay-audit/`. Ainda sem validação visual DX9/GL.
# SK2.17-BU — dummy de item em canais de cast

- Canais de cast Item/ItemExtra agora retêm o dummy configurado e podem usar a matriz do item equipada para posicionar `.eff/.par`, via `TOP_HISTORICAL_ITEM_DUMMY_BIND=1` (padrão desligado).
- A build completa Debug, incluindo `top_gl`, passou; CTest completo 141/141 e CTest focal 2/2 passaram. Fixture sintética passou em ASan/UBSan (`--fixture-only`). O smoke do Client via CTest no sub-build sanitizado apontou para `build/Client`, ausente; o smoke normal Debug passou.
- Dummies do alvo e orientação completa do item permanecem pendentes; não houve validação visual DX9/OpenGL.
- Detalhes e referências DX9/OpenGL: `MIGRATION_SK2_17BU_PLAYER_ITEM_DUMMY.md`.

## SK2.17-BV — dummy/raiz de efeitos recebidos

- Eventos `SkillHit` agora carregam a identidade do ator atingido. Com `TOP_HISTORICAL_TARGET_DUMMY_BIND=1` (padrão desligado), o canal Target acompanha a translação do dummy DX9 correspondente; dummy ausente/inválido usa a raiz do mesmo ator, conforme `CCharacter::SelfEffect`.
- Dados Client (`skillinfo.txt`, colunas 62/63 conforme `SkillVisual.h`): 223 registros com efeito de alvo; 130 com dummy `>=0`, 93 com `-1`.
- Matriz do monstro é calculada a partir do pose `.lab` selecionado para o frame atual; o bridge coleta após `ActorSkin::build`. A coordenada traduz DX9 XYZ/Z-up para GL X/altura/Y. Orientação completa não está portada.
- Build Debug incluindo `top_gl`: exit 0; CTest: 141/141. Fixture ASan/UBSan `top_ui05g_tests --fixture-only`: passou. O alvo sanitizado `top_actor_skill_pose_tests` não linkou (`top::Font::glyph` ausente), então não foi executado com sanitizers. Auditoria Client: 1.031 PAR, 1.227 EFF, 3.476 sistemas, 2.832 partes, 27 gaps/307 IDs de skill estáticos, deriva 0/falhas 0. Sem comparação visual; fidelidade visual não validada.
- Evidências e limitações: `MIGRATION_SK2_17BV_TARGET_DUMMY.md`; patch incremental `SK217BV.patch`.

## SK2.17-BW — guarda histórica de dummy em ItemEffect

- O DX9 rejeita `ItemEffect` quando `sItemDummyLink == -1` (`Character.cpp:836-864`; callsites `STAttack.cpp:262-263`). `TOP_HISTORICAL_ITEM_EFFECT_REQUIRE_DUMMY=1` aplica essa mesma condição somente a canais Item/ItemExtra no GL; desligado por padrão.
- Client `skillinfo.txt`: 50 canais Item/ItemExtra em 46 skills; 3 canais com dummy `-1` nos IDs 0086/0087/0098. `top_fx_coverage_audit` agora registra dummy e linhas de evidência DX9/GL em `fx_skill_item_channels.csv` e no resumo JSON.
- Fixture de efeito cobre opção desligada/ligada e dummy válido; fixture do auditor verifica a regra DX9, os dois callsites, gate/filtro GL e CSV.
- Build Debug incluindo `top_gl`: exit 0; CTest: 141/141. Fixtures de efeito e auditor passaram sob ASan/UBSan (`detect_leaks=0`, `UBSAN_OPTIONS=halt_on_error=1`; auditor executado da raiz para resolver caminhos relativos).
- Relatório: `MIGRATION_SK2_17BW_ITEMEFFECT_DUMMY_GUARD.md`; patch incremental: `SK217BW.patch`.

## SK2.18-A — início de resultado mágico na chegada

- DX9 chama `Play(1)` para o `.par` Result no `Stop()` do projétil e renderiza esse controlador com `FrameMove(0)`. O GL carregava o excesso do `dt` de chegada na idade inicial; `TOP_HISTORICAL_MAGIC_RESULT_FRAME_START=1` agora inicia em idade zero, opt-in.
- O Client vincula Frozen Arrow (skill 93/Magic 20) a `icearrowh.par` e `icearrowh.eff`; o PAR atual tem três sistemas e atraso de 0,1 s no sistema MODEL/type 6 do resultado. Associação de tabela não prova execução real.
- Auditorias atuais: 1.031 PAR/1.227 EFF/3.476 sistemas, zero falhas estruturais; 31 EFF strict-only recuperáveis no parser diagnóstico (30 com referência PAR direta; `fboomerff.eff` sem consumidor estático conhecido). Nenhum asset foi removido.
- Configuração/build Debug com `top_gl`: exit 0; CTest **141/141**; teste focal ASan/UBSan passou (`detect_leaks=0`). Sem comparação visual. Detalhes e patch: `MIGRATION_SK2_18A_MAGIC_RESULT_FRAME_START.md`, `SK218A.patch`.
# SK2.18-B — `Part_fshade` por atualização DX9

- Novo `TOP_HISTORICAL_FSHADE_FRAME_COUNT=1`, desligado por padrão. Para modo 3, o acumulador reproduz o teste estrito `> 0,6 s`, zera o excesso e acrescenta no máximo uma cópia por chamada; `shadeAt(age)` segue disponível como compatibilidade.
- Magic 18 de `Client/scripts/table/magicsingleinfo.txt` usa `04000009.eff`, velocidade 15 e modo 3. O índice estático associa o voo aos IDs 13, 81, 248 e 325; associação não prova uso em runtime.
- Build Debug completo, `top_gl` e CTest completo executados: 141/141 aprovados. Não houve execução visual DX9/OpenGL; sem alegação de fidelidade visual.
- Relatório: `MIGRATION_SK2_18B_FSHADE_FRAME_COUNT.md`. Patch incremental: `SK218B.patch`.

## SK2.18-C — auditoria dos efeitos de Frozen Arrow, Illusion Slash, Taunt e Berserk

- Auditoria atual Client: 1.031 PAR, 1.227 EFF, 3.476 sistemas, 2.832 partes EFF; 27 gaps estáticos/307 IDs de skill, deriva 0 e 0 falhas estruturais. `fx_decode_failures.csv` vazio; 31 falhas do decoder estrito foram listadas individualmente em `fx_eff_usefulness.csv` e recuperadas pelo parser diagnóstico; 30 têm referência PAR direta, `fboomerff.eff` segue sem consumidor estático direto/runtime provado.
- Lacuna concreta confirmada: modo 2 `Part_trace` do Frozen Arrow usa posição/alvo fixos no GL. DX9 acompanha `nTargetChaID`, termina por proximidade/terreno e dispara `Result` no `Stop`; os eventos `Flight` do GL não carregam índice do alvo. Esta etapa só documenta, não altera gameplay.
- IDs 81 e 3 de Illusion Slash são registros distintos; Taunt 242 e Berserk 84 têm gaps de controladores identificados, mas nenhuma dessas lacunas isolou ainda o erro visual relatado.
- Relatório: `MIGRATION_SK2_18C_SKILL_EFFECT_AUDIT.md`. Não houve compilação, CTest ou comparação visual. A próxima correção candidata naquele ponto era `Part_trace`; ela foi implementada com switch opt-in na etapa SK2.18-D abaixo.
## SK2.18-D — controlador `Part_trace` acompanha alvo

- DX9 `Part_trace` renderiza antes do movimento, limita passo a 1,5, termina por proximidade planar ou terreno e acompanha `nTargetChaID` (`arquivo/fontes/Client/src/EffectObj.cpp:243-312`; `arquivo/fontes/Engine/include/MPParticleSys.h:48-53`). O porte adiciona uma timeline OpenGL com tradução do plano DX9 X/Y para GL X/Z e altura DX9 Z para GL Y.
- `TOP_HISTORICAL_PART_TRACE=1` ativa apenas voos mode 2 com altura absoluta e alvo conhecido; desligado por padrão. `PlayCombat` passa índice do alvo e o loop GL amostra raízes após construir o pose do ator. O Result usa a posição de parada/colisão. `pendingAttack_` continua no tempo fixo inicial e a sincronização do dano ainda diverge.
- Auditoria Client atual: 1.031 PAR, 1.227 EFF, 3.476 sistemas, 2.832 partes EFF; 27 gaps/307 IDs de skill, drift 0 e zero falhas estruturais. Dependências mode 2: 122 linhas/59 IDs únicos potencialmente alcançáveis; alcance estático, não runtime comprovado.
- Build Debug incluindo `top_gl`: exit 0; CTest **142/142**. Fixture focal ASan/UBSan: 1/1. Sem sessão Client ou comparação visual DX9/OpenGL; fidelidade não validada.
- Relatório técnico e checklist: `MIGRATION_SK2_18D_PART_TRACE_TARGET.md`. Patch incremental: `SK218D.patch`.

## SK2.18-E — colisão Part_trace usa grade do DX9

- `CMagicEff::HitTestMap` chama o wrapper `CGameScene::GetGridHeight(float,float)`, que passa `int(world*2)` a `MPMap`; este lê um dos quatro quadrantes `btBlock` (`Scene.h:536-540`; `MPMap.cpp:1131-1137`). O porte agora usa `MapFile::sceneGridHeightAt(x,z)` sob `TOP_HISTORICAL_PART_TRACE=1`. A primeira entrega E omitiu o wrapper e foi corrigida no código, relatório e patch; `range2GroundHeightAt` não é a consulta geral da cena.
- Fixture distingue quatro quadrantes desiguais, tiles vizinhos e superfície visual; falhou na primeira implementação E antes da troca para o leitor correto. Build Debug completo com `top_gl`: exit 0; CTest 142/142; focal Part_trace/terreno 2/2; ASan/UBSan focal 1/1. `SK218E.patch` foi substituído pela versão corrigida; quem aplicou a primeira versão pode usar somente `SK218E_CORRECAO.patch`.
- Auditorias Client atuais: FX 1.031 PAR/1.227 EFF/3.476 sistemas/2.832 partes EFF, 27 gaps/307 skills, drift 0 e falhas estruturais 0; PAR 1.031 decodificados/0 falhas. As 31 recusas EFF strict foram individualizadas no CSV: 31 recuperadas por decode diagnóstico, 30 com PAR direto, uma sem referência direta, nenhuma com runtime provado. Nenhum arquivo removido.
- Sem execução de Client ou comparação visual DX9/OpenGL. O leitor de mapa GL não reproduz a janela ativa de `GetGridHeight` fora da área visível; esse limite permanece. Relatório: `MIGRATION_SK2_18E_PART_TRACE_GRID_COLLISION.md`; patch incremental: `SK218E.patch`.

## SK2.18-F — flag de vínculo de modelo e auditoria Taunt

- O auditor PAR agora preserva `_bModelDir` (v14+) e acrescenta `model_direction` no fim do CSV. Runtime GL ainda pula esse campo. Client: 3.256 sistemas false/220 true; 955 MODEL/STRIP `.eff` sem billboard e com flag false, mas sem prova de vínculo runtime para todos.
- `cfsf.par` sistema 0 e `cfsj.par` sistema 0 têm STRIP, billboard false, modelDirection false e offset (0,0,1). O DX9 passa o vínculo aos filhos, mas estes são billboards: `MPModelEff.cpp:820-840` usa a câmera e substitui o centro pelo vínculo. O GL soma indevidamente `keyframe.position` quando o PAR pai não é billboard (`EffectSystem.cpp:2933-2946`). O inspector EFF agora expõe o intervalo de posições: ambos variam (0,0,0)→(0,0,1). Taunt e Roar: 2 IDs estáticos.
- Etapa somente audit: sem mudança de renderer. Build Debug completo incluindo `top_gl`: exit 0; CTest 142/142; fixture PAR ASan/UBSan 1/1. Reaudit FX: 27 gaps/307 skills, drift 0/falhas 0; as 31 recusas EFF strict-only mantêm as classificações individuais, sem assets descartados.
- Relatório: `MIGRATION_SK2_18F_TAUNT_BONE_BINDING_AUDIT.md`; patch incremental `SK218F.patch`. A diferença de centro vinculado foi comprovada estaticamente; sua contribuição ao erro visual ainda depende de comparação DX9/GL. A orientação completa dos filhos não-billboard é outra lacuna e não deve ser generalizada aos filhos billboard de Taunt.

## SK2.18-G — centro de billboard vinculado no motor compartilhado

- `TOP_HISTORICAL_BOUND_BILLBOARD_CENTER=1` recupera a substituição da posição filha por centro do dummy para MODEL/STRIP elegível (`MPParticleSys.cpp:1205-1227,1526-1531`; `MPModelEff.cpp:820-840`). Sem condição por ID de skill. Default desligado; requer dummy disponível em actor/item/target e `_bModelDir=false`.
- Novo `top_effect_binding_audit`: 1.880 combinações resolvidas, 22 candidatas em 8 PARs/8 IDs estáticos (11,26,95,214,235,242,243,265). Não prova execução nem vínculo runtime. Outros 13 canais não resolvidos: 7 caminhos PAR literais ausentes e 6 IDs sem sceneffect; investigar antes de classificar como inúteis.
- Fixture de formato cobre MODEL/STRIP, actor/target/item, direção habilitada, ausência de dummy e consistência com o audit. Build Debug completo com `top_gl`: exit 0; CTest **143/143**; ASan/UBSan focal 1/1. Sem comparação visual DX9/GL.
- Relatório e comandos: `MIGRATION_SK2_18G_BOUND_BILLBOARD_CENTER.md`; patch incremental `SK218G.patch`. Permanecem pendentes a matriz completa dos filhos não-billboard, outros donos de efeito e sincronização visual/dano de voos dinâmicos.

## SK2.18-H — tabela BIN/TXT e canais sem recurso

- Novo audit somente-leitura `Tools/scene_table_audit.py`: extrai layout/colunas/contrato de autenticação do fonte DX9 fornecido e autentica `sceneffectinfo.bin` antes de interpretar os registros. O padrão DX9 usa BIN; GL usa TXT. Client real: 208 bytes/registro, 14.000 slots, 1.093 registros nos dois modos, zero diferenças nos campos semânticos comparados.
- Os 13 canais não resolvidos de G continuam ausentes nos dois modos: 7 com nomes literais `heallight.pa`/`01000017.par`, 6 sem registro 221/880. O DX9 não corrige extensão. ID 221 no servidor não autoriza importar dados para o Client. Catálogo completo: 73 nomes sem asset literal, zero casos somente de caixa; execução runtime não comprovada. Sem mudança de renderer/Client ou remoção de EFF.
- Debug completo com `top_gl`: exit 0; CTest 144/144, incluindo novo audit Python com 10 casos sintéticos. Sem novo ASan/UBSan (sem alteração C++) e sem comparação visual. Relatórios em `build/scene-table-audit-sk218h`; detalhes em `MIGRATION_SK2_18H_SCENE_TABLE_BINARY_AUDIT.md`; patch incremental `SK218H.patch`.

## SK2.18-I — composição de matriz para filhos não-billboard

- `TOP_HISTORICAL_BOUND_MODEL_MATRIX=1`, desligado por padrão, compõe a matriz real do dummy para filhos EFF não-billboard de MODEL/STRIP não-billboard e `_bModelDir=false`. Aplica transformação filha/escala do sistema/parte linear do osso, depois translação + offset em mundo; não soma o range/ângulos do caminho MoveTo ao vínculo. Regra compartilhada em actor/target/item, sem IDs no renderer.
- Audit de vínculos: 181 combinações candidatas em 26 PARs e 34 IDs estáticos (inclui Illusion Slash 3/81), com coluna nova `non_billboard_binding_candidate`; dummy/runtime continuam não comprovados. Os 8 candidatos billboard de G permanecem separados. FX atual mantém 27 gaps/307 IDs, drift 0/falhas estruturais 0: não declarar tipo inteiro completo por esta correção.
- Fixture inicial falhou no runtime anterior; nova fixture de PAR/EFF/LGO cobre matriz afim, chave filha e exclusões. Debug incluindo `top_gl`: exit 0; CTest 145/145; ASan/UBSan focal e vizinhos 5/5. Sem comparação visual; Illusion Slash continua aproximado. Relatório: `MIGRATION_SK2_18I_BOUND_MODEL_MATRIX.md`; patch incremental `SK218I.patch`.

## SK2.18-J — rastreio e prévias com vínculo explícito

- Novos switches, ambos default off: `TOP_HISTORICAL_SKILL_PREVIEW_BIND=1` identifica dono/alvo conhecidos da prévia; `TOP_HISTORICAL_BINDING_TRACE=1` registra matriz, ramo e geometria emitida. Preserva índices originais PAR/EFF após filtros do loader. Ferramentas `run_binding_previews.py` e `binding_trace_audit.py` distinguem prévia, observação de geometria e aprovação visual.
- OpenGL executado sob Xvfb fora da restrição de sockets do sandbox, com assets reais, renderização por software e saves copiados: 44 prévias + 3 janelas tardias, todas exit 0/captura; 180/203 combinações candidatas G/I observadas com dummy e regra aplicada, abrangendo os 41 IDs. Não comprova combate nem concordância artística. Hashes originais estáveis durante cada bateria; dois saves diferem entre o snapshot anterior e o início das baterias, sem autoria comprovada e sem restauração desses arquivos.
- Restam 23 combinações não observadas: 8 barradas pelo limite GL de oito filhos no ID 282 e 15 ainda sem causa isolada. FX mantém 27 gaps/307 IDs, drift 0/falhas 0; PAR 1.031/zero falhas; 31 EFF strict-only continuam individualizados, sem runtime provado e sem remoção.
- Debug completo com `top_gl`: exit 0; CTest 147/147; ASan/UBSan focal G/I/J 4/4; regressões GPU de blend e filtro exit 0. Sem comparação DX9/GL: Illusion Slash aproximado. Detalhes: `MIGRATION_SK2_18J_BINDING_RUNTIME_OBSERVATION.md`; patch incremental `SK218J.patch`.

## SK2.18-K — quantidade de filhos EFF

- `TOP_HISTORICAL_EFF_PART_COUNT=1`, default off, remove os cortes de compatibilidade de 8 filhos aninhados e 16/16/8 filhos billboard/nativos/externos de magia. Usa a quantidade aceita pelo decoder existente; o limite de segurança de 64 e as verificações de malha/textura permanecem. Sem ajuste de escala.
- Novo `fx_eff_part_counts.csv` extrai limites do código atual e cruza contagens com PARs/grafo: 54 EFF potencialmente truncados no caminho aninhado, cinco IDs alcançados. Doze desses arquivos são strict-only, individualizados e sem IDs de skill atribuídos no grafo; não foram removidos nem se habilitou decode permissivo automaticamente.
- A/B com mesmo executável em dez prévias GL isoladas: 33 filhos adicionais em seis EFF/cinco IDs (95,223,282,285,290), todos os processos exit 0/captura. Saves originais estáveis durante cada bateria. É observação de prévia, sem combate ou comparação visual DX9 comprovados.
- Fixture inicial falhou antes da mudança. Debug completo com `top_gl`: exit 0; CTest 149/149; ASan/UBSan G/I/K 4/4. FX mantém 27 gaps/307 IDs, drift 0/falhas 0; 31 EFF strict-only preservados. Relatório `MIGRATION_SK2_18K_AUTHORED_EFF_PART_COUNT.md`; patch incremental `SK218K.patch`. Restam as 15 combinações não observadas de J que não eram o limite de oito.


## SK2.18-M — movimento naval fora do sistema de efeitos

- Movimento do barco passa a usar soma de velocidades padrão de `shipinfo`/`shipiteminfo` carregada em `ActorSkin`, em vez de `10.f` fixos. Guppy: dado padrão 450 → 4,5 m/s (`/100` é inferência coerente com conversão terrestre).
- Evidência e limitações: `MIGRATION_SK2_18M_BOAT_SPEED.md`. Componentes melhorados não estão representados pela UI offline.
- `top_gl` Debug foi compilado (exit 0). `--map magicsea --check` verificou a asserção do Guppy sem acusá-la, mas terminou exit 1 na exigência de dois atores. CTest e comparação visual DX9 não executados.
