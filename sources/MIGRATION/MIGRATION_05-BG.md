# 05-BG — coeficientes de dano físico de skills

A auditoria de combate encontrou uma heurística de dano baseada em procurar textos como `1.5` e `0.5` dentro do nome da callback. Isso não representa o corpo da fórmula Lua e fazia algumas skills receberem incrementos de nível incorretos ou cair no dano mágico genérico. O carregador agora identifica callbacks comprovadas na tabela original e usa seus coeficientes exatos:

| Skills / famílias | Callback Lua | Dano antes da defesa/resistência | Fonte |
|---|---|---|---|
| Ataques-base melee/ranged/staff | `Skill_Melee_End`, `Skill_Range_End`, `Skill_Staff_End` | `Atk_Dmg` | linhas 252, 289 e 314 |
| Double Shot (90) | `Skill_Lzj_End` | `(1.5 + nível × 0.15) × Atk_Dmg` | linha 1010 |
| Frozen Arrow (93) | `Skill_Bdj_End` | `1.2 × Atk_Dmg` | linha 1037 |
| Howl (107) | `Skill_Lh_End` | `(1 + nível × 0.05) × Atk_Dmg` | linha 1907 |
| Meteor Shower (112) | `Skill_Lxjy_End` | `(0.5 + nível × 0.1) × Atk_Dmg` | linha 1089 |
| Backstab (86) | `Skill_Guz_End` | `(1 + nível × 0.1) × Atk_Dmg` | linha 2112 |
| Hunter Strike (91) | `Skill_Ldc_End` | `(2 + nível × 0.2) × Atk_Dmg` | linha 5686 |
| Penetrating Bullet (115) | `Skill_Ctd_End` | `(1 + nível × 0.2) × Atk_Dmg` | linha 6184 |
| Inferno Blast (61), Dispersion Bullet (114) | `Skill_Ssd_End` | `Atk_Raise(1.2 + nível × 0.15)` | linha 6228 |
| Barbaric Crush (108) | `Skill_Ymsl_End` | `Atk_Raise(1.5 + nível × 0.1)` | linha 6023 |
| Cripple (94) | `Skill_Tj_End` | `1 + (nível × 0.05) × Atk_Dmg` | linha 2337 |

Os coeficientes são vinculados pelo identificador exato da callback em `skillinfo.txt`, sem executar Lua no cliente. `Atk_Raise` multiplica o ataque antes da aplicação de defesa/resistência, diferente de `Atk_Dmg`, então tem ramo de cálculo separado. `Play::selfTest` verifica callback/ID e vários níveis contra os coeficientes. O caminho genérico não aplica mais um multiplicador mágico às callbacks físicas nomeadas acima. A regra genérica anterior não é considerada prova de equivalência para outras skills.

Validação: build de `top_gl`, `top_ui05b_tests`, `top_gl --check`, suíte CTest completa (70/70) e `git diff --check` passaram.

Limite: permanecem skills sem callback classificada, estados, área, resistência, bônus especiais, orientação/condição de Backstab e a relação pose→quadro de impacto; este incremento melhora fórmulas identificadas sem declarar o combate completo.
