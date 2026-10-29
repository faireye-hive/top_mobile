# Tales of Pirates OFFLINE — migração 05-AY

## Objetivo

Continuação incremental sobre a **05-AX**. Implementar **tickets de viagem com destinos históricos comprovados** e **seis baús Relic de armadura condicionados ao tipo do personagem**, preservando as missões, UIDs, inventário, UI procedural e saves. Nenhum arquivo de `Client/` ou `Client/user/` foi alterado.

## Fontes históricas verificadas

- `Antigo(4).zip`: `servidor/server/GameServer/resource/script/calculate/variable.lua`, bloco `TicketSys.Ticket` e `Server.Sys.Ticket.Active`.
- `Antigo(4).zip`: `servidor/server/GameServer/resource/script/birth/birth_conf.lua`, entradas `AddBirthPoint`.
- `Antigo(4).zip`: `fontes/Server/GameServer/src/NpcScript.cpp` (`lua_MoveCity`) e `Character.cpp` (`CCharacter::MoveCity`): o servidor escolhia um ponto de nascimento da cidade, com pequena variação aleatória.
- `scriptsclientatual(2).zip`: `scripts/offline/script/calculate/ItemEffect.lua` (`Ticket`, seis caixas `ItemUse_SCBox*`) e `scripts/table/iteminfo.txt` (IDs e callbacks).
- `scripts/offline/script/shop/tickets.lua` descreve itens vendidos, **não é o cadastro dos destinos**; os destinos vêm de `TicketSys.Ticket` + `birth_conf.lua`.

## Tickets — 45 IDs auditados

- `Engine/include/top/ItemTicketRules.h` e `Engine/src/item/ItemTicketRules.cpp`: catálogo puro, independente de UI, OpenGL, NPC e `Play`.
- **45 tickets têm cidade e coordenadas verificadas**. A configuração também contém ID **3141**, mas seu destino é `Name = ""`; por segurança, esse ticket permanece indisponível. Itens que exibem texto “ticket” mas **não têm callback `Ticket`** não são transformados em teleportes.
- A auditoria atual encontrou 47 registros `iteminfo` com callback `Ticket`: 45 têm destino portado (`ticket_destination_PORT_APPROX`), 3141 não tem destino e 3024 (`Church Visit Ticket`) é um evento temporal separado, sem entrada em `TicketSys.Ticket`; este último segue pendente e não foi convertido em teleporte comum.
- Destinos comuns: Argent City (`4602`), Shaitan City (`4603`), Icicle Castle (`4604`), Thundoria Castle (`3048/3828`), Spring Town (`332/3076`), Autumn Island (`583`), Summer Island (`563`), Lone Tower e minas; mapas de `jialebi` só funcionarão quando existirem no acervo do jogador.
- Requisitos históricos: ao menos **50% de HP e SP**, personagem **não pode estar em barco**, e mapas `garner2`/`prisonisland` proíbem tickets. Os registros históricos atualmente têm `Level.Min`/`Max` como `nil`; não inventamos limites de nível.
- O GameServer permite vários pontos de chegada de uma mesma cidade. Para a migração offline, é utilizado **o primeiro `AddBirthPoint` histórico** de cada cidade, sem o deslocamento aleatório de até ~2 unidades do servidor original; isso é uma adaptação deliberada, não identidade perfeita de spawn.
- Na UI, o clique no item apenas prepara a viagem. `Platform/linux/main.cpp` usa a infraestrutura existente de transição de mapas e **só após sucesso** consome 1 unidade do ticket correspondente, buscando seu **UID** no inventário recarregado e gravando o save. Se o mapa de destino estiver ausente, o mapa falhar ao carregar ou houver restrição de uso, **nada é consumido**.
- `Play` permanece como fachada para exibir mensagens, verificar HP/SP e registrar a transição; a tabela e validações estão no módulo `item/`. Nenhum novo campo de save.

## Baús Relic de armadura — IDs 209–214

`ItemChestRules.cpp` agora contempla:

| Item | Tipo admitido | Mínimo de slots livres antes do uso | Recompensas de `GiveItem` (IDs, quantidade 1) |
|---:|---|---:|---|
|209 Night | 1 | 3 | 395, 587, 747 |
|210 Hercules | 2 | 4 | 397, 829, 603 |
|211 Progeny | 1 ou 3 | 3 | 399, 589, 749 |
|212 Wise/Panda | 3 / 4 | 4 | tipo 3: 401, 591, 751 / tipo 4: 403, 593, 753, 2218 |
|213 Fairy/Gold Fish | 3 / 4 | 4 | tipo 3: 405, 595, 755 / tipo 4: 407, 597, 757, 2220 |
|214 Tidal/Dragon | 1, 3 / 4 | 4 | tipo 1 ou 3: 409, 599, 759 / tipo 4: 412, 601, 761, 2222 |

- Os seis **reutilizam a transação da 05-AX**: pré-requisito de slots livres, simulação de retirada/inserção em cópia, validação de todos os IDs de recompensa, commit único, UIDs estáveis, persistência e eventos `TE_GETITEM`.
- Tipo de personagem incompatível = recusa clara, sem consumir o baú.
- **Limitação preservada:** o parâmetro extra `95` passado a `GiveItem` **ainda não é reproduzido** como metadado de instância. Os IDs/quantidades são fiéis, mas bônus especiais podem diferir do histórico; não declarar equivalência completa de equipamento.
- O sistema de sorteios e baús de iniciante (`NewbieChest`) **não está implementado**; nem todo baú segue o modelo de recompensa fixa.

## Arquitetura e testes

- Nova biblioteca `top_item` inclui `ItemTicketRules.cpp` além das regras já presentes.
- `PlayItems.cpp` contém apenas a integração com bolsa e estado do personagem; nenhuma execução de Lua.
- `Play.h` oferece `takeTicketTravelRequest`, `consumeTicketAfterTravel` e `reportTicketTravelFailed` para que a camada Linux confirme viagem antes de consumir.
- Adicionado `tests/UI05AYTests.cpp`, com regras de tickets, casos de sucesso/recusa, destinos duplicados e caixas condicionadas. `Play::selfTest` passou a cobrir abertura real de armadura e ticket por UID, preservando save/reload.
- **CTest headless: 68/68 testes aprovados**; compilação dos targets `top_regression_tests`, `top_ui05v_play_tests`, `top_ui05ay_tests` confirmada. Sintaxe C++17 dos arquivos novos e `Platform/linux/main.cpp`: aprovada.
- **OpenGL integral e viagem visual não executados aqui** porque o SDK GL/GLX não está disponível neste ambiente; testar no Ubuntu. O sucesso headless não prova disponibilidade dos mapas do cliente.

## Testar no Ubuntu

Antes de tudo, faça backup de `Client/user/` e substitua apenas a pasta `sources/`.

```bash
rm -rf build
cmake -S sources -B build -DTOP_BUILD_UI_TESTS=ON
cmake --build build -j4
ctest --test-dir build --output-on-failure
./build/top_gl --check
./build/top_gl
```

No jogo, confira: `05-AY` no log; bilhete de Argent `--gm-item 4602` com HP/SP acima e abaixo de 50%, viagem Argent→Argent mantendo destino histórico e consumo **de somente uma unidade**; bilhete para outro mapa presente (por exemplo 3048 Thundoria no `garner` ou 563 Summer no `darkblue`); bilhete para mapa ausente não desaparece. Teste baús 209–214 para tipos corretos, tipo errado e bolsa cheia, e reinicie para conferir saves e UIDs.

## Próxima etapa — 05-AZ

Continuar revisão dos **itens históricos**, incluindo `NewbieChest`, sorteios com configuração original, e semântica de `GiveItem(..., 95)` conferida nos fontes do GameServer. Depois: validar **stats** do personagem; por último **skills, poses, animações e efeitos**. Preservar a arquitetura modular `mission/npc/item/ui/gl`.
