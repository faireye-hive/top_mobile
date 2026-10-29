# Migração 05-BV — efeito de Shadow Slash

Shadow Slash (skill 86, `Skill_Guz_End`) já tinha o dano físico classificado, mas não aplicava `STATE_XY`, o atordoamento do callback. Agora o estado é aplicado no quadro de impacto, depois do dano: duração `3 + floor(nível/2)` segundos, dobrada quando a diferença de direção satisfaz a condição Lua; alvos com HP máximo ≥100.000 recebem a redução histórica `floor(duração/2)+1`. No mapa Garner2, os alvos da família UnNormal também usam o fator `AGI/200` do callback.

Atordoar cancela ataque/projétil pendente do alvo, evitando que um monstro continue o golpe já iniciado durante o estado. O estado usa o mesmo relógio e bloqueio de movimento/ataque já usado por outros callbacks.

O autoteste carrega a regra de `skillinfo.txt`, verifica `Skill_Guz_End` e os coeficientes `1 + nível×0,1`, executa o impacto no nível 5 e confirma 15 de dano, 10 s de stun por ângulo e cancelamento do ataque pendente. Build `top_gl`, `top_gl --check`, CTest 70/70 e `git diff --check`.

Não foram portados nesta etapa os modificadores de set Black Dragon, exceções de bosses de Abaddon ou resistência de outros estados; eles continuam sendo efeitos separados para auditoria.
