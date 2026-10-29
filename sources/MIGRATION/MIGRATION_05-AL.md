# Tales of Pirates — Migração 05-AL: missões históricas simultâneas

## Escopo e base

Implementado sobre a **05-AK.3**, mantendo o jogo offline em Linux/OpenGL, a UI procedural, NPC Lua passivo, equipamentos/forja por UID e os recursos de `Client/` intactos.

O usuário validou a 05-AK.3 em seu Ubuntu: `--startup-profile` mediu **1.227,71 ms até o primeiro quadro** e **60,98 ms** na preparação dos marcadores de missão (05-AK.2: 2.725,11 ms e 1.545,99 ms, respectivamente). Nesta etapa **não foi introduzida nova otimização de startup**.

## Funcionalidades

1. **Até 8 missões históricas ativas** por personagem, inclusive missões oferecidas pelo mesmo NPC. Rejeita IDs repetidos e não permite reaceitar uma missão já concluída no histórico retido.
2. Cada missão controla seus próprios dados: ID, título, objetivo, destino, alvo, quantidade abatida, itens exigidos, recompensa e estados ativa/concluída.
3. Ao derrotar um monstro, todas as missões de caça elegíveis progridem. A conclusão ocorre individualmente, somente se objetivos, materiais e capacidade de bolsa permitirem; recompensas e consumo de materiais não são duplicados para missões diferentes sem materiais suficientes.
4. Missões de destino ou de coleta podem ser concluídas ao atingir suas condições. O sistema de XP, ouro e inventário original é reutilizado.
5. **Diário paginado:** até cinco entradas por página, seleção por clique, objetivos da missão selecionada, botões Anterior/Próxima e Abandonar apenas para missão histórica ativa selecionada. **As três missões locais pré-existentes permanecem listadas** e não são abandonáveis.
6. O painel de diálogo dos NPCs e seus botões Loja/Forge permanecem disponíveis; missões ativas e concluídas ganham estados individuais; marcadores `!`/`?` são calculados consultando a lista de missões ativas.

## Persistência retrocompatível

- Preservado **sem alterar a ordem** o campo de missão da geração anterior do save (`historical_`), seguido por todos os campos `Y1`, `T1`, `W1`, `Z1`, `ZT1` anteriores.
- Adicionados **dois campos opcionais ao fim da linha**: `Q2:` codifica as missões históricas adicionais; `QH2:` preserva título e descrição da missão do campo legado. Strings em hexadecimal e listas de materiais delimitadas evitam conflitos com `|`, `;` e vírgulas anteriores.
- Saves 05-AK.3 ou anteriores continuam legíveis. Os novos campos são ignoráveis por leitores legados, **mas versões antigas não preservam as missões adicionais ao regravar o save**; recomenda-se backup de `Client/user/` antes de voltar uma versão.
- Leitura defensiva: limite de 128 entradas históricas adicionais retidas, 8 ativas, comprimento limitado, validação numérica, textos hexadecimais e rejeição de IDs duplicados.

## Testes e validação

- **55/55 testes CTest aprovados no ambiente headless** com scripts históricos montados para o teste de integração.
- `Play::selfTest` cobre três missões simultâneas (duas caças ao mesmo monstro), progresso separado, salvamento, leitura, títulos/objetivos, conclusão, abandono direcionado, missões locais e limite de 8 ativas.
- Novo teste `ui05al_parallel_quest_diary` valida o desenho procedural do diário e o texto dos controles de paginação.
- Compilação de sintaxe C++17 de `Play.cpp`, `ActorSkinUIBridge.cpp` e `Platform/linux/main.cpp` aprovada.
- **A compilação integral e a inspeção visual OpenGL ainda precisam ser feitas no Ubuntu**, porque este ambiente não dispõe de `GL/gl.h`/`libGL.so` de desenvolvimento.

## Limitações conhecidas para a próxima etapa

- O modo offline continua interpretando **parte** das condições históricas dos scripts Lua (não executa integralmente as regras do GameServer).
- O histórico completo de missões extras fica limitado a 128 registros persistidos, mesmo concluídos. A próxima etapa deve introduzir um registro compacto de IDs concluídos para evitar saturação sem permitir repetição indevida.
- O diário exibe objetivos resumidos por missão; ainda não reproduz todas as regras e etapas condicionais do original.
- Conclusão de missões é automática quando os requisitos locais estão satisfeitos, comportamento já existente no protótipo offline e não necessariamente idêntico à entrega ao NPC do jogo original.

## Como testar no Ubuntu

Faça cópia de `Client/user/` e extraia o pacote na raiz do projeto (mantendo `Client/`):

```bash
cmake -S sources -B build -DTOP_BUILD_UI_TESTS=ON
cmake --build build -j4
ctest --test-dir build --output-on-failure
./build/top_gl
```

Converse com dois NPCs oferecendo missões diferentes, aceite mais de uma, confira a lista paginada do diário e tente abandonar somente a selecionada. Para caças compartilhadas, verifique que derrotar uma criatura conta separadamente nos dois objetivos. Reinicie para testar o save. O usuário pode continuar usando `--startup-profile`; não é necessário outro benchmark nesta etapa.
