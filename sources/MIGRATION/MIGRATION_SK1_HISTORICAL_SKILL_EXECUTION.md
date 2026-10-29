# SK1 — Execução histórica genérica de habilidades (primeira etapa)

Entrada: skillinfo.txt colunas 9 (type), 10 (helpful), 16 (distance cm), 17 (target). O motor puro `HistoricalSkillExecution` reconhece alvo inimigo (type 0 ou 1, helpful=0, target=4), passivo (type=2), outras classes (modo anterior; autoexecução de aliado, self/solo não migrada nesta etapa).

Fluxo `castSkill` → `requestedSkill_` → `pickFoe` → `chasePoint` → `resolve` → `executeSkill` → timeline já existente de animação/efeitos/dano. O comando não consome SP nem cooldown até que o alvo esteja no alcance histórico; após o cast, não há perseguição ou ataque automático. O estado pendente não é persistido em saves; é preservado por rebind de mundo no mesmo personagem.

A auditoria `top_skill_execution_audit` relata 411 registros, dos quais 244 com alvo inimigo, 40 passivos e 127 de outras classes neste acervo. Há 314 habilidades com canais visuais declarados e 97 sem; isto NÃO significa que todos os recursos .par/.eff sejam interpretáveis. O fluxo novo não inclui habilidades que precisem selecionar aliado/terreno nem completa suas fórmulas Lua.

Testes: UI05SK1Tests (outra habilidade ID 3 em execução one-shot), UI05BITests (Illusion Slash 81), UI05VPlayTests (regressão histórica), UI05F/05G/05K (efeitos). Verificar os testes com a pasta Client e a versão OpenGL integral no Ubuntu.

Build:
```
cmake -S sources -B build -DTOP_BUILD_UI_TESTS=ON
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
./build/top_skill_execution_audit Client/scripts/table/skillinfo.txt
./build/top_skill_execution_audit Client/scripts/table/skillinfo.txt --csv > auditoria-skills.csv
```
