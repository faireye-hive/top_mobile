# 05-CB — Fórmula histórica da skill Harden

`Play::castSkill` tratava Harden (ID 225) como um buff genérico de `+18 DEF` por 12 segundos. O callback real `Skill_Shpf_End` aplica `STATE_SHPF` por 180 segundos; `State_Shpf_Add` acrescenta `14 + (nível da skill × 4)` à defesa. O consumo de SP e cooldown continuam vindo das funções e colunas históricas de `skillinfo.txt`.

O client agora reconhece `Skill_Shpf_End` pelo nome exato do callback, guarda o bônus numérico e o remove junto com o estado ao expirar. Saves antigos sem metadados ainda recebem o callback conhecido pelo ID 225; callbacks desconhecidos continuam sem inferência genérica. O HUD exibe o bônus real, e o estado persiste através do rebind local do roster.

Fontes: `Client/scripts/offline/script/calculate/skilleffect.lua` (`Skill_Shpf_End`, `State_Shpf_Add`) e `Client/scripts/table/skillinfo.txt` (skill 0225).

Regressão: skill histórica em nível 3 deve aplicar +26 DEF, consumir o SP calculado pela tabela e expirar após 180 segundos. Build `top_gl`/`top_ui05v_play_tests`, CTest direcionado, `top_gl --check` e `git diff --check` aprovados.
