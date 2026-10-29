# PORT — Lucky Packet (3901)

## Regra histórica

`ItemEffect.lua::ItemUse_HYBOX` exige um espaço livre. Sorteia EXP: em 1/1000 concede 8.888 ouro e 66.600 EXP; nos demais casos concede 8 ouro e sorteia 100, 150, 200, 250 ou 300 EXP. A partir do nível 80, a EXP sorteada é dividida por 50. Em seguida há chance de 1/500 de sortear uma das 21 cartas ativas, uniformemente, na tabela `BaoXiang_HYBOX` em `variable.lua`. Toda abertura entrega também o Red Paper (3904) qualidade 5.

## Implementação OpenGL

A tabela é carregada pelo leitor compartilhado de recompensas aleatórias. O handler calcula o jackpot, EXP, carta opcional e Red Paper, então simula a remoção do pacote e a inserção de todos os resultados numa cópia da bolsa. Só aplica a transação se os itens couberem e os dados de progressão estiverem disponíveis; atualiza ouro/EXP, aplica progressão local de nível quando necessário, registra os itens e salva.

## Limites conhecidos

Status `PORT_APPROX`: o RNG local não compartilha a sequência Lua do servidor. A aplicação de CEXP usa a progressão local para manter níveis e pontos coerentes. Ouro acima do limite persistido local de 1.000.000.000 é saturado. Fluxo e save/reload ainda precisam de confirmação no jogo.
