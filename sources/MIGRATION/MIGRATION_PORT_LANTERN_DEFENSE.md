# PORT — Lantern defense consumables (3107, 3342)

## Regra histórica

`ItemUse_DenglongA` (3107) aplica `STATE_DENGLONG`, nível 1, por 900 segundos em terra. `ItemUse_Denglong` (3342) aplica o mesmo estado por 600 segundos. `State_DengLong_Add` soma 50 à defesa e o callback recusa o uso enquanto o personagem navega. O modo `ADD_LARGER` histórico não renova um estado já ativo no mesmo nível.

## Implementação OpenGL

Os dois itens atualizam uma instância de estado temporário própria de consumíveis. O bônus entra no cálculo derivado de defesa; a contagem expira junto dos outros estados de item e é preservada na troca de mapa. O fluxo não altera estados nem efeitos de skills.

## Limite conhecido

O estado é runtime-only, como os outros buffs temporários locais, e não sobrevive ao reinício do cliente. Build confirmado; o uso e o save/reload ainda precisam ser exercitados no jogo.
