# Migração 05-BQ — fórmula histórica da habilidade Heal

## Evidência e mudança

O callback `Skill_Xzy_End` de `Client/scripts/offline/script/calculate/skilleffect.lua` calcula o valor negativo de HP como `-floor(50 + 50*sklv + Sta(atker)*7)` e aplica `HP_Endure_Dmg` ao alvo. O caminho de `castSkill(97)` usava anteriormente `12 + bsta*2`, uma aproximação que subestimava a cura e ignorava nível da skill e bônus de STA equipado.

O cliente offline agora usa a fórmula Lua com o nível aprendido e `STA = bsta + bônus de equipamento`. Como o jogo não tem outros personagens-alvo controlados localmente, Heal resolve para o próprio personagem, limita a cura ao HP máximo e mantém o custo/cooldown extraídos da skillinfo. Não concede EXP de cura: o original só concede essa EXP ao curar outro jogador, situação que não existe no modo solo atual.

## Testes

- Fórmula pura: nível 1/STA 5 = 135; nível 5/STA 12 = 384.
- Integração de cast: nível 1, STA base 5 + STA do equipamento 2, HP 10 → HP 159; custo histórico 29 SP e cooldown ativo.
- Regressão do fluxo de pose usa a ação lida de `skillinfo.txt` para Heal, em vez de presumir a pose genérica 11.
- Build `top_gl`, `top_gl --check`, CTest 70/70 e `git diff --check`: aprovados.

## Limite do porte

Seleção de alvo para curar outro jogador, experiência por cura em grupo e efeitos visuais completos da habilidade ainda não existem no modo offline solo. Outras skills continuam necessitando auditoria callback a callback; esta etapa não declara equivalência global.
