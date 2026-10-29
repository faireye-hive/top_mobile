# Tales of Pirates OFFLINE — migração 05-AX

## Escopo

Incremento sobre **05-AW**, sem reiniciar o projeto nem alterar `Client/` ou `Client/user/`. O objetivo desta etapa é restaurar **oito baús de recompensa determinística** conforme `scripts/offline/script/calculate/ItemEffect.lua` e `Client/scripts/table/iteminfo.txt`.

## Alterações

- Novo módulo **`Engine/include/top/ItemChestRules.h` / `Engine/src/item/ItemChestRules.cpp`**, integrado à biblioteca `top_item`. Regras sem dependência de OpenGL, UI, `NpcSystem` ou `Play`.
- O dispatcher em `Engine/src/item/PlayItems.cpp` consulta o plano apenas quando **ID do item E nome do callback Lua histórico** coincidem. Outros callbacks, inclusive sorteios e scripts desconhecidos, continuam sem execução e sem consumo indevido.
- O script exige slots livres **antes** da abertura; o código respeita a exigência original. Em seguida simula retirar uma unidade do baú e inserir todas as recompensas em uma **cópia** da bolsa. Se alguma inserção falhar ou faltar metadado do item premiado, a operação inteira é cancelada.
- Somente no êxito a bolsa é trocada, os UIDs são ajustados, o evento `TE_GETITEM` das recompensas é enviado às missões e a gravação existente é solicitada. Sem novo campo no save.
- **Correção visual do log:** `Platform/linux/main.cpp` agora anuncia `05-AX`, em vez do texto legado `05-AV` que permaneceu na 05-AW.

## Baús implementados (valores conferidos no ItemEffect.lua histórico)

| iteminfo ID | Callback | Regra/resultado |
|---:|---|---|
| 192 | `ItemUse_SaintCloth` | Exige **4 slots vazios**. Ganha 825, 826 e 827; tipo de personagem **4** ganha também 828. |
| 215 | `ItemUse_SCBoxRYBZ` | 1 slot vazio; ganha 109. |
| 216 | `ItemUse_SCBoxXMC` | 1 slot vazio; ganha 111. |
| 217 | `ItemUse_SCBoxLQJ` | 1 slot vazio; ganha 113. |
| 218 | `ItemUse_SCBoxLYJ` | 1 slot vazio; ganha 115. |
| 219 | `ItemUse_SCBoxFHG` | 1 slot vazio; ganha 117. |
| 220 | `ItemUse_SCBoxLXHZ` | 1 slot vazio; ganha 119. |
| 221 | `ItemUse_SCBoxYCJ` | 1 slot vazio; ganha 150. |

**Atenção:** o valor `95` do quinto argumento de `GiveItem` nos scripts dos baús Relic é um parâmetro histórico adicional; esta etapa concede os IDs/quantidades, **não afirma restaurar esse metadado** no item gerado. Estudar semântica no GameServer original antes de ampliar o modelo de instância.

## Testes executados neste ambiente

- Configuração CMake com `TOP_BUILD_UI_TESTS=ON`: sucesso.
- Build `top_regression_tests`: sucesso.
- **CTest 67/67 aprovados** (`ui05ax_historical_deterministic_chests` adicionado).
- `Play::selfTest()` como `ui05v_play_state_regression`: sucesso, testando abertura, bolsa cheia, catalogação de recompensas, UID e concessão para tipo 4.
- `g++ -std=c++17 -fsyntax-only` em `Play.cpp`, `PlayItems.cpp` e `ItemChestRules.cpp`: sucesso.
- **Não compilado/executado o `top_gl` gráfico** neste ambiente porque faltam `/usr/include/GL/gl.h` e `/usr/lib/.../libGL.so`; o usuário compilará e validará no Ubuntu.
- Patch `05-AW → 05-AX` aplicado a uma cópia limpa da 05-AW: comparar hashes SHA-256 de todos os arquivos com a nova árvore.

## Testes recomendados no Ubuntu

Faça backup de `Client/user/`; substitua somente `sources/` e execute:

```bash
rm -rf build
cmake -S sources -B build -DTOP_BUILD_UI_TESTS=ON
cmake --build build -j4
ctest --test-dir build --output-on-failure
./build/top_gl --check
./build/top_gl
```

No inventário, use `--gm-item 215` (se suportado pela linha de comando) ou o comando GM existente para adquirir os baús sem modificar a tabela; testar: abrir caixa Relic com 1 slot livre, recusar com 0; baú Kylin exigir 4 slots antes, sem recompensa duplicada; reabrir o save e conferir quantidades e UIDs. Comprove visualmente que a versão da tela inicial é 05-AX.

## Limitações e próximos passos

- **Não reproduzir Lua arbitrário**. `NewbieChest`, `Ticket`, `GiveRandomItems`, armaduras Relic condicionadas por classe, `AddLifeSkill`, pet/fairy/boat permanecem sem equivalente direto. `Ticket` no script depende de `Server.Sys.Ticket.Active` e tabelas `TicketSys`; não inventar teleporte.
- Continuar análise de `iteminfo.txt`, `ItemEffect.lua` e GameServer histórico; implementar próximos grupos simples com regras auditadas e rollback atômico.
- Itens de missão, forja, equipamentos, UIDs, oito missões simultâneas e saves antigos preservados.
- Depois dos itens, auditar stats; por último skills, poses e efeitos.
