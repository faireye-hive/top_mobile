# PORT — Enchanting Card Chest (1035)

## Regra histórica

`Client/scripts/offline/script/calculate/ItemEffect.lua::ItemUse_MoHuanBox` exige pelo menos um espaço livre. O primeiro sorteio é `math.random(1, 4663)`:

- 1–3: carta 3866;
- 4–14: carta 3864;
- 15–65: carta 3858;
- 66–4663: novo sorteio uniforme `math.random(3850, 3875)`; 3858, 3864 e 3866 são redirecionadas para 3850.

Todas as entregas usam qualidade 4.

## Implementação OpenGL

O handler do item 1035 replica as faixas e IDs, exige o espaço livre antes do sorteio e executa consumo e recompensa numa cópia da bolsa. Só troca o estado real, atualiza eventos de item e salva depois de confirmar que os dados da recompensa estão no catálogo e a inserção foi concluída. A lista histórica 3850–3875 está presente em `iteminfo.txt`.

## Limite conhecido

Status `PORT_APPROX`: a distribuição foi transcrita, mas o gerador local não compartilha a sequência de `math.random` do servidor. O consumo e o save/reload precisam ser conferidos no jogo.
