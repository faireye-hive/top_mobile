# 05-BC — Recompensas ponderadas: GiveRandomItems e BlackMarket

Os callbacks históricos `GiveRandomItems` e `BlackMarket` agora leem seus mapas (`rItem` e `Market`) de `ItemEffect.lua` e as entradas ativas das tabelas em `variable.lua`. Os 55 itens `iteminfo` com `GiveRandomItems` e o ID 9600 (`MysteryBox`), também explicitamente mapeado dentro da função histórica, continuam usando as 52 tabelas carregadas. Outros 35 itens Black Market usam as tabelas ponderadas definidas para seus IDs, também sem executar Lua.

Ao abrir, o jogo exige ao menos um espaço livre como o callback antigo, sorteia pela soma e ordem dos pesos `Rad`, simula o consumo e todas as recompensas em uma cópia da bolsa, preserva a caixa em caso de dados ausentes ou espaço insuficiente, registra os itens recebidos e salva o resultado. Os pacotes 3902 e 3903 também concedem o item adicional 3904 conforme o script histórico.

## `PORT_APPROX` — limitações abertas

- A distribuição ponderada vem dos dados históricos, mas usa o RNG offline do `Play`; a sequência de `math.random` do processo original não foi reproduzida.
- `Quality` é carregado e persistido no campo de qualidade da pilha. Geração de atributos de instância para equipamentos (`chInstance`) dentro desses baús não foi comprovada e continua pendente.
- `GoodItem` gera uma mensagem local ao jogador; o anúncio mundial do servidor não existe no modo offline.
- `BlackMarket` tem as mesmas aproximações de sequência aleatória, criação de atributos `chInstance` e anúncio mundial. Se a bolsa não comportar a recompensa, o cliente preserva o item de entrada para evitar perda; o callback Lua não declara um teste de espaço explícito.
- O item 9600 declara `MysteryBox` em `iteminfo`, mas aparece como `rItem[9600] = BaoXiang_MysteryBOX` dentro de `GiveRandomItems`; o uso dessa tabela compartilhada é uma inferência explícita marcada como aproximação.
- Datas, limites de uso e efeitos de estado específicos de algumas caixas continuam pertencendo a callbacks próprios e não são inferidos por este porte.

Fontes históricas: `Client/scripts/offline/script/calculate/ItemEffect.lua` (`GiveRandomItems`, `BlackMarket`) e `Client/scripts/offline/calculate/variable.lua` (tabelas ponderadas). A auditoria 05-AW classifica as respectivas entradas como `random_reward_table_PORT_APPROX` e `black_market_table_PORT_APPROX` enquanto as limitações acima permanecerem.
