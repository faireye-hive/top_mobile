# Migracao 05-Q — habilidades ofensivas de monstros

- `characterinfo.txt` colunas 44/45: habilidades e pesos associados por indice.
- `skillinfo.txt` colunas 16/59/60: distancia, efeito de voo e velocidade do projetil.
- Seleciona apenas habilidades ofensivas, validas e ao alcance; monstro sem entradas validas continua usando golpe normal.
- Perseguicao usa maior alcance das habilidades validas; o golpe valida novamente seu alcance particular e linha de visao no impacto.
- O identificador da habilidade acompanha as mensagens `Cast`, `Flight` e `SkillHit` para o catalogo de efeitos 05-F+.
- As formulas Lua especificas de dano e buffs nao foram portadas; dano segue o calculo offline existente.
- Projeteis sao efeitos visuais, nao simulam fisica ou colisao propria.
- Protege comportamento de IA 11, retorno ao spawn e zona segura ja existentes.
- Para validar: `cmake -S sources -B build-ui-tests -DTOP_BUILD_UI_TESTS=ON`,
  `cmake --build build-ui-tests -j4`,
  `ctest --test-dir build-ui-tests --output-on-failure`.
- No Linux do usuario compilar `top_gl`, rodar `--check` e comparar efeitos visuais, especialmente Shadow Hunter (skill 138).
