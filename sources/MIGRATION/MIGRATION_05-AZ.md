# Tales of Pirates OFFLINE — migração 05-AZ

## Escopo

Incremento **05-AY → 05-AZ**, sobre os fontes completos recebidos, sem alterar `Client/`, saves ou motor OpenGL. Prioridade: caixas `NewbieChest` do `ItemEffect.lua` histórico; preservar tickets, baús Relic, missões e arquitetura modular.

## Fontes originais comparadas

- `scriptsclientatual(2).zip`: `scripts/offline/script/calculate/ItemEffect.lua`, função **`NewbieChest(role, Item)`**, aproximadamente linhas 965–1234.
- `scriptsclientatual(2).zip`: `scripts/table/iteminfo.txt`, IDs **436–443**, todos com callback `NewbieChest`.
- `Antigo(4).zip`: `fontes/Server/GameServer/src/CharScript.cpp`, `lua_GiveItem`: o quinto parâmetro Lua é **`byType`**, repassado como `byAddType` a `CCharacter::AddItem`.
- `Antigo(4).zip`: `fontes/Server/GameServer/src/Character.cpp`, `CCharacter::GiveItem` chama **`ItemInstance(byAddType, ...)`**. Portanto números `4`, `11` e `95` NÃO são quantidades de item nem bônus fixos de atributo; seus metadados de instância não foram integralmente migrados. É incorreto anunciar equivalência completa desses equipamentos.

## Itens implementados

- Novo módulo **`Engine/src/item/ItemNewbieChestRules.cpp`** e cabeçalho `Engine/include/top/ItemNewbieChestRules.h`. O catálogo é **puro**: não usa `Play`, GL, UI nem executa Lua em runtime. Valida o par `(id,callback)`, nível, profissão, tipo de personagem e barco.
- IDs **436–442** agora abrem ao clicar, desde que cumpram as condições históricas. Ouro e todas as recompensas passam por um único commit em `item/PlayItems.cpp` após simular retirada do baú e inserção de todos os itens.
- Recompensas dependentes das famílias de profissão `1/8/9`, `2/12`, `4/16`, `5/13/14` e tipo do personagem, conforme Lua; combinações não representadas são recusadas sem consumo.
- O requisito de vagas usa `GetChaFreeBagGridNum` **antes de consumir** a caixa. Também checa a capacidade final efetiva, sem deixar ouro ou recompensas parciais se algum ID não existir ou faltar vaga.
- Preservação de UID, `Q2`–`Q5` e saves antigos, evento `TE_GETITEM` para cada item recebido e teto preventivo de ouro offline já utilizado pelo motor (1 bilhão).

| ID | Nome histórico | Nível mínimo | Vagas livres exigidas | Ouro recebido | Itens comuns / próxima caixa |
|---:|---|---:|---:|---:|---|
|436|Newbie Chest|5|4|2.500|437, 9, 4308, 444, 6050|
|437|Winged Chest|10 + primeira classe|9|5.000|438, tickets 4602×10, 4603×10, 4604×10, 445 e equipamentos por profissão/tipo|
|438|Flowery Chest|15 + classe|6|7.500|439, 4264, 3351, 3352, 3353, 446 e equipamentos|
|439|Researching Chest|20 + classe|6|10.000|447, 440, 3844 e equipamentos|
|440|Smelting Chest|25 + classe|5|12.500|441, 448, 262 e equipamentos|
|441|Exploring Chest|30 + classe|5|15.000|442, 449 e equipamentos|
|442|Guiding Chest|35 + classe|3|17.500|443, 451|
|443|Advancement Chest|40 + **segunda promoção suportada**|4|200.000|recompensas históricas por classe, `GiveItem(...,95)`, `MakeItem(...,11)`, 450 e 855×49|

### Limitações preservadas

1. A tabela de 443 é suportada apenas para as segundas classes com recompensas explícitas no Lua (jobs 8, 9, 12, 13, 14 e 16); classes desconhecidas continuam recusadas sem consumir a caixa.
2. Para as recompensas de 443, os eventos 11/95 usam rolls locais determinísticos derivados do UID. A qualidade 3, a quantidade/seleção ponderada de afixos e os atributos-base de armas 1–4/7/9 e armaduras 22 são materializados e salvos. Isso reproduz os dados e a progressão offline, mas não a sequência aleatória do servidor nem todos os modos `ItemInstance` para outros tipos/eventos.
3. Para a caixa 436, o Lua exige **4 espaços antes de abrir** apesar de conceder **5 IDs**: o próprio baú libera 1 espaço quando consumido; uma prévia de inventário valida todos os itens antes de aplicar.
4. Nível é **mínimo**, não um teto. Caixas antigas continuam utilizáveis quando o personagem tem nível maior e atende às outras restrições.
5. Em vez de reproduzir o risco histórico de conceder parcialmente quando a bolsa fica cheia, o offline preserva a caixa e faz rollback completo por segurança.

## Alterações e verificação

- Adicionados `ItemNewbieChestRules.h/.cpp`, registrados na biblioteca independente `top_item` em `CMakeLists.txt`.
- `PlayItems.cpp`: entrada transacional para `NewbieChest` antes da lógica de outros baús; não adiciona regra de item em `NpcSystem.cpp` nem no renderizador.
- `tests/UI05AZTests.cpp`: testes de oito IDs, nível/promoção/barco/espécie/callback, recompensas de 443 por classe e tags 11/95.
- `Play::selfTest`: teste real do baú 443, geração estável por UID, atributos, ouro, UIDs e save/reload; demais caixas continuam com transação integral.
- Atualizada identificação de inicialização em `Platform/linux/main.cpp` para 05-AZ.
- **CTest headless: 69/69 testes passaram**; `top_ui05v_play_tests` e `top_ui05az_tests` aprovados. Não foi executado o renderizador integral neste ambiente (SDK GL/GLX indisponível).
- Patch incremental verificado contra ZIP completo 05-AY com comparação SHA-256 de todos os arquivos.

## Testar no Ubuntu

Backup de `Client/user/`; atualizar somente `sources/`.

```bash
rm -rf build
cmake -S sources -B build -DTOP_BUILD_UI_TESTS=ON
cmake --build build -j4
ctest --test-dir build --output-on-failure
./build/top_gl --check
./build/top_gl
```

Teste com `--gm-item 436` em personagem nível 4 e depois nível 5, conferir recompensas/ouro e reinício. Teste com bolsa quase cheia e em barco (caixa não deve desaparecer). No nível 10, caixa 437 exige profissão; verifique a classe e os três pacotes de 10 tickets. No nível 40, valide o baú 443 com uma segunda classe suportada e compare os afixos após reiniciar.

## Próxima migração — 05-BA

Auditar `ItemInstance(byAddType)` no GameServer e implementar primeiro as semânticas seguras dos modos 4/11/95 nas instâncias offline; só então habilitar 443 se reproduzir `MakeItem` e classes de segunda promoção. Continuar catálogo de caixas e consumíveis de forma incremental, depois **stats**, por último **skills/animações/efeitos**.
