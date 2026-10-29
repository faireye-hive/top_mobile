# 05-BM — callbacks de dano mágico

O fallback genérico de habilidades mágicas não correspondia aos callbacks Lua de Lightning Bolt (ID 214, `Skill_Lj_End`) e Conch Ray (ID 216, `Skill_Bkcj_End`). O client agora classifica os callbacks pelo nome exato e aplica as fórmulas usuais do original:

- Lightning Bolt: `floor(80 + sklv*10 + Sta(atker)*6 + Lv(atker)*3)`.
- Conch Ray: `floor(200 + sklv*30 + Sta(atker)*6) * 1.1`.

`Sta(atker)` usa o atributo base mais o bônus de STA do equipamento. Lightning Bolt também aplica `STATE_MB` nível 2 por 3 segundos, reduzindo deslocamento e aumentando o intervalo de ataque, tanto no jogador quanto em monstros. Os testes exercitam os dois sentidos do impacto. Os cálculos estão cobertos por casos de skill level/STA/nível e pela verificação de que os IDs históricos carregam os callbacks esperados.

Em Garner2, quando o alvo está na lista histórica `UnNormalMonster_ID`, ambos usam agora `MAGIC_Atk_Dmg`: ataque mágico baseado nos atributos de item `MnatkIb/MxatkIb`, bônus de STA por job, `2*STA(defender)`, resistência e diferença de nível. Lightning Bolt aplica `floor(MAGIC_Atk_Dmg * sqrt(sklv))`; Conch Ray aplica `floor(MAGIC_Atk_Dmg + sklv*200)`. O Conch Ray também aplica `STATE_XY` nível `sklv` por 1 segundo quando `random(1,200) >= CON(defender)`. O blackout interrompe movimento, ataques e casts offline, e a mesma regra funciona para jogadores ou monstros como alvo.

Os coeficientes `MnatkIa/MxatkIa` vindos dos equipamentos agora entram no cálculo normal de ataque mínimo/máximo (radix 1000), e são persistidos junto aos bônus individuais de cada item. Isso não altera `MAGIC_Atk_Dmg`: esse callback especial lê explicitamente `MnatkIb/MxatkIb` e os bônus de STA por classe, sem consultar os coeficientes nem `MnatkSa/MxatkSa` ou buffs `MnatkSb/MxatkSb`. Permanecem pendentes, para paridade desse ramo especial, `MLCH`, bônus de fada/equipamento Black Dragon e experiência de navegação; os demais multiplicadores e estados precisam ser integrados apenas onde o Lua correspondente realmente os aplica. Os testes cobrem a leitura do item, fórmula normal com coeficiente, compatibilidade de saves antigos e fórmulas base do ramo especial.

Validação: build de `top_gl`, `top_gl --check`, CTest (70/70) e `git diff --check`.
