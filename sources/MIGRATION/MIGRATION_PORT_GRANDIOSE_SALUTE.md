# PORT — Grandiose Salute (1011)

## Regra histórica

`ItemUse_HHLP` recusa uso navegando. No horário local 1 de janeiro, das 22h até 23h (inclusive), delega a `ItemUse_XNBOX`. Esse callback exige nível 40 e pelo menos um espaço livre, então sorteia um voucher segundo a tabela New Year Chest de `ItemChestRules.cpp`.

## Implementação OpenGL

O item 1011 agora compartilha a tabela ponderada e a transação já implementadas para o New Year Chest 2307. A checagem usa a hora local, igual a `os.date` no script histórico. Uso fora do período ou enquanto navega preserva o item.

## Limite conhecido

Status `PORT_APPROX`: a distribuição é histórica, mas o RNG local não compartilha a sequência Lua do servidor. O evento sazonal ainda precisa ser verificado durante sua janela ativa.
