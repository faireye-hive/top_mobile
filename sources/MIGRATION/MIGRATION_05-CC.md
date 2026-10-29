# 05-CC — Callback histórico Enfeeble

O callback `Skill_Sj_End` (skill 95) agora usa seu dano literal de `100 + nível × 10`, sem fórmula genérica de magia ou crítico. No nível 3, o impacto causa 130. A tabela histórica continua fornecendo o custo de SP e o cooldown (26 + nível e 15 segundos).

No alvo, Enfeeble aplica por `5 + floor(nível/2)` segundos os estados equivalentes a `STATE_SJ` e `STATE_JNJZ`: reduz em 20% os ataques mínimo/máximo do monstro e impede a seleção de skills, mantendo o ataque básico. O estado expira, limpa o nível aplicado e libera novamente as skills. Alvos com HP máximo a partir de 100.000 recebem a duração curta histórica `floor(3 + nível×0,3)`; o desvio aleatório de 1% do servidor não é reproduzido no cliente solo.

Fontes históricas já copiadas no cliente Linux: `Client/scripts/offline/script/calculate/skilleffect.lua` (`Skill_Sj_End`, `State_Sj_Add`) e `Client/scripts/table/skillinfo.txt` (skill 0095). Nenhum arquivo de `arquivo/` foi adicionado neste incremento.

Regressões em `Play::selfTest` conferem callback, SP/cooldown, dano de nível 3, duração, redução de dano recebido, silêncio de skills, ataque básico e reativação das skills ao expirar.
