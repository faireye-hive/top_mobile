# Migração 05-BS — callbacks de Will of Steel e Berserk

Foram portados dois estados temporários a partir de `Client/scripts/offline/script/calculate/skilleffect.lua`:

- `Skill_Gtyz_End` (skill 63, Will of Steel): +3 DEF por nível aprendido durante 15 s. O bônus participa do dano recebido e é removido ao expirar.
- `Skill_Kb_End` (skill 84, Berserk): ASPD aumenta por `0.20 + 0.015 × nível` durante 20 s. Como o combate local representa velocidade pelo intervalo de ataque, o intervalo usa o recíproco desse multiplicador. O estado e o fator são limpos ao expirar.

Os dois callbacks são reconhecidos pelo nome histórico da fórmula em `skillinfo.txt`; o ID 84 mantém fallback apenas para saves/dados sem callback carregado. Os testes cobrem fórmula, níveis, defesa efetiva, duração e interação do intervalo de Berserk com ataque preparado.

Validação: compilação `top_gl`, `top_gl --check`, CTest 70/70 e `git diff --check` aprovados.

Isso não fecha o porte de skills: estados de alvo, acumulação/substituição de efeitos, dispel, resistência e os demais callbacks ainda precisam de implementação e validação individual. Animações/efeitos visuais por habilidade também continuam sendo auditados separadamente das regras de dano.
