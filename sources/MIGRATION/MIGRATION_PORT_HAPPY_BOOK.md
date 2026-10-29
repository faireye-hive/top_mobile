# Porte de ItemUse_HappyBook

Os itens históricos 853 (Happy Holiday Magazine) e 2989 (Super Sushi) executam `ItemUse_HappyBook` em `Client/scripts/offline/script/calculate/ItemEffect.lua`.

O cliente OpenGL agora reproduz as faixas do callback usando os limiares cumulativos `DEXP` de `Client/scripts/offline/calculate/variable.lua` e traduz o alvo para EXP relativa ao nível, formato usado pelos saves locais. O fluxo aplica os pontos de atributo e habilidade dos níveis alcançados, restaura HP ao subir de nível e salva o resultado.

Regras preservadas do histórico:

- uso bloqueado enquanto navega;
- níveis 1–9: alvo de EXP cumulativa do nível atual +5;
- níveis 10–29: alvo do nível atual +3;
- níveis 30–59: alvo no próximo nível;
- níveis 60–75: 50% do intervalo até o próximo nível, mais 10 EXP;
- níveis 76–85: 30% do intervalo, mais 10 EXP;
- nível 79: correção histórica de 2% do excedente que ultrapassa o próximo limiar;
- nível 86 ou superior: uso recusado e item preservado.

O porte não altera fórmulas nem efeitos de skills. A auditoria registra esses dois IDs como `happy_book_supported`. Os testes exercitam avanço de cinco níveis, persistência e recusas no barco e acima do limite.
