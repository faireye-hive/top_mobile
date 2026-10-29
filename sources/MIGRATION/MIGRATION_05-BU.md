# Migração 05-BU — LUK, crítico e ficha de combate

`AttrCalculate.lua` calcula o crítico como `11 + floor(LUK × 3 × Crt_luk_rad[classe])`; Hunter (job 2) usa coeficiente `0,25`, e os demais jobs usam `0,31`. O cliente copiava o crítico do template visual da raça, ignorando essa fórmula.

O LUK inicial agora vem da coluna histórica `Luck` de `characterinfo.txt` (fallback 5), o bônus direto de LUK dos equipamentos vem de `iteminfo.txt` (`Lukconstant bonus`, coluna 57), e o crítico derivado é recalculado após bônus de equipamento/classe. Não foi criado um sexto atributo distribuível: o LUK continua sendo dado de raça/equipamento, como indica o modelo de cinco pontos distribuíveis preservado pelo save.

A janela de Status passou a mostrar atributos totais com equipamento, LUK, ataque mínimo/máximo finais, DEF, resistência física, HIT, esquiva, crítico e regeneração HP/SP, além de ASPD. Os campos de ataque não exibem mais a diferença de coeficiente da classe como se fosse o ataque final. Tooltips de equipamento também listam bônus de LUK. A ordenação dos cinco atributos distribuíveis permanece FOR, CON, AGI, SPR e ACC.

Testes: fixture confirma leitura/agregação da coluna LUK; autoteste compara o Newbie nível 1 (LUK 5 → CRT 15), Hunter nível 1 (CRT 14) e equipamento com LUK/crítico; teste visual confirma os novos rótulos. Build `top_gl`, `top_gl --check`, CTest 70/70 e `git diff --check` passaram.

Limite: fama/drop rate, bônus de passivas/estados e progressão integral de classes ainda não estão cobertos por esta etapa.
