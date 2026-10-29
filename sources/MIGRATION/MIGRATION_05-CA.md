# 05-CA — Callback histórico de canhão e queda de dano por distância

`Skill_Paodan_End` (skills 27, Cannon Shot; e 233, Bombardment) ainda usava o dano genérico da IA. A implementação offline agora reproduz o caminho observado em `Client/scripts/offline/script/calculate/skilleffect.lua`: `Fire_Dmg`/`Pow_Dmg`, dano mínimo de `floor(nível do atacante * 0,25) + 1`, seguido da queda `floor(dano * (1 - min(distância_metros * 0,1, 1)))`.

A regra pura fica em `top::play_combat::monsterFireDamageWithDistance`, separada da orquestração em `PlayCombat.cpp`; `loadSkillRules` associa ambos os IDs ao callback exato. Assim, o dano diminui com a distância e chega a zero no limite de 10 m previsto pela fórmula, sem inferir o comportamento pelo nome traduzido.

Regressões adicionadas:

- Classificação histórica dos IDs 27 e 233.
- Valores de dano base/mitigado e queda em 2, 9 e 10 m.
- Resolução integrada do ataque do monstro com Cannon Shot a 0,3 m (77 de dano no fixture).

Verificação local: `top_ui05v_play_tests`, build de `top_gl`, CTest 71/71, `top_gl --check` e `git diff --check` aprovados. Isso valida a regra e os recursos carregados pelo check; não substitui comparação visual/sessão gráfica nem conclui a cobertura de todas as skills.
