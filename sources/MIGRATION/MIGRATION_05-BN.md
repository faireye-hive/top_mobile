# 05-BN — callbacks físicos diretos dos monstros

Além dos callbacks baseados em `Atk_Dmg` documentados em 05-BL, foram classificados pelo nome exato mais seis callbacks usados pela tabela histórica de skills:

| ID | Callback | Dano migrado |
|---:|---|---|
| 283 | `Skill_BlackHyz_End` | `Phy_Dmg(2 * random(Mnatk, Mxatk))`, mínimo 20 |
| 288 | `Skill_hqgj_End` | `Phy_Dmg(1.5 * random(Mnatk, Mxatk))`, mínimo 20 |
| 299 / 310 | `Skill_wllk_End` / `Skill_wlfd_End` | `Phy_Dmg(random(Mnatk, Mxatk))`, mínimo 20 |
| 305 | `Skill_wlsj_End` | `Phy_Dmg(800 + random(Mnatk, Mxatk))`, mínimo 20 |
| 325 | `Skill_HyzHX_End` | `2.5 * clamp(floor(Aspd/70), 1, 3) * Atk_Dmg`; callback fixa `sklv=10` |

Esses callbacks usam `Hp_Endure_Dmg`, portanto não recebem teste de esquiva/acerto crítico que o Lua não chama. Os testes verificam os vínculos dos IDs, o coeficiente/floor e os três níveis do multiplicador de HyzHX.

Limites remanescentes: `Check_Ys_Rem` e outros estados laterais não estão incluídos; `characterinfo` fornece intervalo/tempo de ataque e não `Aspd` cru, então o callback 325 estima `Aspd` pela taxa de ataque OpenGL. Estados, resistência a estados e callbacks não classificados continuam pendentes. Validação da etapa: build `top_gl`, `top_gl --check`, CTest (70/70) e `git diff --check`.
