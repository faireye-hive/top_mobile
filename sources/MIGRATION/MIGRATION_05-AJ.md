# Tales of Pirates offline / OpenGL — Migração 05-AJ

Base: **05-AI**. Meta: retomar funcionalidades históricas offline de NPCs e
missões, mantendo renderização, animações, equipamentos, itens por UID,
forja e saves sem alterações.

## 1. Problema funcional corrigido

`Platform/linux/main.cpp::interactNpc` interrompia a busca por missões no
primeiro `NpcSystem::mission()` válido, **mesmo quando**
`Play::offerHistoricalMission()` retornava `false` (por nível inadequado,
missão já concluída ou outra indisponibilidade). Assim, missões posteriores
na lista do mesmo NPC ficavam inacessíveis.

Agora a busca utiliza `firstAvailableNpcMission()`: tenta cada ID na ordem
histórica (máximo 128 por interação), pula missões indisponíveis e interrompe
somente após oferecer uma válida. A descrição da missão só entra no diálogo
quando a oferta realmente foi criada.

**Limitação mantida:** o offline oferece uma missão por interação e não
executa o runtime Lua/GameServer. Missões cujos requisitos ainda não são
suportados continuam sendo uma pendência, não ganham regras inventadas.

## 2. Leitura dos scripts de NPC

Antes, `NpcSystem::catalog`, `dialog` e `missionIds` interpretavam linhas
isoladas e paravam ao encontrar qualquer `end`, inclusive de condições dentro
de uma função. A procura por texto simples podia interpretar comentários ou
nomes de chamada parecidos como dados históricos.

`Engine/include/top/NpcLuaData.h` adiciona um parser **passivo** baseado no
tokenizador do 05-AE, agora também capaz de preservar literais de texto para
as falas. Reconhece chamadas diretas de `Talk`, `InitTrade`, `Weapon`,
`Defence`, `Other`, `Boat`, `Pet`, `Item`, `AddNpcMission`, inclusive com
quebra de linha e blocos internos; ignora comentários, strings que apenas
mencionam esses comandos, chamadas `Obj.X`, funções aninhadas, expressões
em vez de IDs e funções de nome semelhante. Funções incompletas não são
aceitas. IDs históricos `0290` são lidos como decimais `290`.

A implementação não executa scripts Lua, não avalia condições arbitrárias,
não libera novos serviços, não altera `NpcForgeServices` ou probabilidades
do jogo. A API de `NpcSystem` não mudou.

Os dados analisados são compartilhados por `catalog`, `dialog` e
`missionIds` via cache de instância. O cache é invalidado em
`NpcSystem::load()` (troca de mapa). Edições externas dos arquivos Lua
em tempo real exigem recarregar o mapa para atualizar os dados.

## 3. Testes e compatibilidade

- 50/50 testes headless aprovados na versão 05-AJ.
- `ui05aj_npc_menus_and_eligible_missions` cobre funções de nomes parecidos,
  comentários, strings, `if`, `for`, funções aninhadas, argumentos em várias
  linhas, ID com zeros à esquerda, script incompleto, cache e escolha da
  próxima missão elegível.
- Comparação com scripts originais fornecidos em `scriptsclientatual.zip`:
  153 NPCs de Garner, 150 com diálogo, 20 com lojas, 86 com missões ligadas,
  loja `r_trade01` com 78 itens, `r_talk86` com 28 missões — mesma contagem
  do leitor da 05-AI para essa amostra.
- `main.cpp` passou em `g++ -std=c++17 -fsyntax-only`.
- Binário gráfico `top_gl` **não foi compilado por completo** neste ambiente:
  `GL/gl.h` está ausente (SDK OpenGL), não há falha C++ conhecida nesta etapa.

## 4. Instalação e validação no Linux

Faça backup de `Client/user/` antes de atualizar o projeto, especialmente
antes de testar interações que possam alterar o save.

Extraia o ZIP `TalesOfPirates_05-AJ_Completo.zip` na raiz do repositório,
preservando `Client/`; alternativamente, aplique o `.patch` sobre 05-AI.

```bash
cmake -S sources -B build -DTOP_BUILD_UI_TESTS=ON
cmake --build build -j4
ctest --test-dir build --output-on-failure
./build/top_gl --check
./build/top_gl
```

Teste dentro do jogo um NPC com múltiplas missões de níveis diferentes.
Observe que somente uma missão é oferecida por interação. Caso deseje
selecionar explicitamente entre várias missões simultâneas, essa interface
fica para uma etapa posterior.

## 5. Fora do escopo

Os oito mapas com recursos de cena faltando no diagnóstico 05-AI foram
rebaixados de prioridade, a pedido do usuário. Não foram apagados do catálogo
nem alterados nos arquivos do cliente. `binglang2.map` e os recursos
históricos continuam sendo reportados pelos diagnósticos quando ausentes.

Próximas etapas candidatas: missões históricas múltiplas no mesmo diálogo,
condições de missão ainda sem suporte e serviços Lua faltantes, sempre
comparando a implementação com o cliente e GameServer originais.
