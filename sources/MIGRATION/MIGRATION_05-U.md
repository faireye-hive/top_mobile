# Migração 05-U — Atributos históricos, combate físico e progressão

## Problema relatado

O usuário comprovou a aprovação da 05-T (25/25 testes), mas observou que monstros de alto nível tinham pouca vida, não pareciam usar ataque, defesa ou esquiva de suas fichas, e que o jogador ainda não possuía progressão de EXP/HP/SP semelhante à original.

## Causa confirmada no código da 05-T

- `Play::bind()` lia `characterinfo.txt`, mas cortava o HP de **todos** os inimigos em `40 + nivel * 8`. Isso transformava o HP base 4540 de Shadow Hunter (ID 201) em 584 e os 16760 HP do Jungle Guardian (ID 701) em 448.
- `Play::damageOf()` usava um **redutor artificial** sobre dano >8, enfraquecendo bosses.
- `rawDamage()` descontava somente **defesa/5** no ataque do jogador.
- A ficha do jogador calculava HIT e FLEE, mas os golpes não consideravam esquiva, e `Foe` não guardava HIT/FLEE dos monstros.
- `applyAttrs()` subtraía fórmulas usando **o mesmo nível atual**, anulando o ganho automático de HP/SP por nível.
- `expNext_ = 25 * nivel` ignorava as quantidades em `variable.lua`.

## Correções implementadas

1. `HistoricalStats.h` interpreta, sem executar Lua, os valores literais `DEXP[n]` de `Client/scripts/offline/calculate/variable.lua`. A tabela é **cumulativa** no servidor original: no cliente offline, o XP dentro do nível continua sendo armazenado em `exp_` e a exigência para o próximo nível é `DEXP[nivel+1] - DEXP[nivel]`. O leitor usa 64 bits para os valores cumulativos, maiores que 2^31 a partir dos níveis altos.
2. `Play` passa a usar a nova exigência tanto em mortes quanto em conclusões de missão, sem modificar o formato da experiência já salva em disco. Ao não localizar a tabela, mantém `25*nivel` como fallback offline explícito. O limite de nível deste porte permanece 100; sua configuração ainda não é idêntica à do servidor histórico.
3. As fichas de inimigos usam diretamente `characterinfo` (colunas 60,61,62,65,66,68,69,70) para **nível, HP, EXP informado, ataque mínimo/máximo, defesa, HIT, FLEE**. Não há mais cap em `40 + nivel*8`. Há limites amplos de sanidade de parsing, não limites baseados no nível.
4. Os golpes físicos agora usam a redução de defesa de `skilleffect.lua::Phy_Dmg` para **resistência física zero**. O dano mínimo offline ainda é 1. O suporte a resistência física (`PDEF`), crítico e fórmulas individuais de habilidades permanece incompleto.
5. Os ataques básicos passam pelo cálculo de esquiva de `Check_MisorCrt()` do Lua: a chance de MISS é limitada a 90%, conforme a fórmula original. Fichas artificiais de teste sem HIT/FLEE não recebem sorteio de esquiva.
6. O jogador agora aumenta HP e SP por nível segundo os coeficientes de profissão já implementados, comparando com o **nível inicial 1** em vez do mesmo nível corrente. As melhorias de pontos AP/TP e o save continuam ativos. A regeneração de HP/SP usa os máximos atualizados.
7. Ao selecionar um monstro, o HUD apresenta nível, HP, ATK mínimo/máximo, DEF, HIT e FLEE. A tela C mantém as linhas de atributos do jogador.
8. `top_stats_inspect` mostra os atributos de monstros específicos sem inicializar o OpenGL:
   ```bash
   ./build/top_stats_inspect Client 201 701
   ```

## O que NÃO está totalmente reproduzido

- **EXP de monstros**: apenas 8 entre ~1593 fichas com HP/nível válidos contêm EXP positivo na coluna 62 do `characterinfo.txt` disponível. `exp_and_level.lua` consulta `ATTR_CEXP` do monstro no runtime do servidor. **Para evitar zerar a progressão offline de quase todas as criaturas**, usamos o valor positivo explícito quando existe; do contrário, preservamos o fallback `6 + 4*nivel` que já existia na 05-T. Esse fallback **não é uma taxa oficial** e ainda exige investigar como o servidor inicializa o atributo EXP.
- Habilidades Lua de dano/crit/estados, resistência `PDEF`, armaduras complexas e PvP continuam parciais.
- Saves legados guardam EXP **dentro do nível**, não XP cumulativo. O mesmo valor é preservado e passa a ser comparado com o novo requisito. Saves existentes não são sobrescritos na instalação, mas recomenda-se fazer cópia de `Client/user/offline-save-*.txt` antes do teste.
- Os testes automatizados não comparam todos os cálculos de `AttrCalculate.lua`; apenas validam os aspectos migrados. Ainda é necessário conferir a duração do combate nos mapas reais.
- A 05-U foca em atributos e progressão, não em estados temporários durante streaming, que ainda merecem auditoria.

## Exemplos confirmados no cliente disponível

| ID | Nome | Lv | HP | ATK | DEF | HIT | FLEE |
|---:|---|---:|---:|---|---:|---:|---:|
| 1 | Long Haired Guy | 1 | 40 | 4–5 | 3 | 2 | 1 |
| 201 | Shadow Hunter | 68 | 4540 | 434–651 | 114 | 229 | 277 |
| 701 | Jungle Guardian | 51 | 16760 | 548–823 | 163 | 184 | 200 |

`DEXP`: Lv1→2=5, Lv4→5=66, Lv9→10=1234, Lv50→51=1396840.

## Validação

- 26/26 suítes CTest passaram. **As 25 suítes antigas vieram de executáveis testados na 05-T**, copiados para executar a regressão sem recompilar centenas de fontes sem alteração. O teste `ui05u_historical_stats_experience` foi **compilado com código da 05-U** e executado também com os arquivos `Client` reais disponíveis.
- `Play::selfTest()` da 05-U foi **recompilado** e executado com scripts históricos e as novas verificações de HP e nível.
- Sintaxe de `Play.cpp` e `Platform/linux/main.cpp` verificada com C++17.
- `top_stats_inspect` compilou e identificou os atributos citados acima.
- **Compilação completa do top_gl com GLX ainda depende do seu Linux**, pois este ambiente não possui `GL/gl.h`.

## Como testar no Linux

```bash
rm -rf build build-ui-tests
cmake -S sources -B build
cmake --build build -j4
./build/top_gl --check
./build/top_stats_inspect Client 201 701
./build/top_gl

cmake -S sources -B build-ui-tests -DTOP_BUILD_UI_TESTS=ON
cmake --build build-ui-tests -j4
ctest --test-dir build-ui-tests --output-on-failure
```

**Testes manuais prioritários:** aproximar-se de um chefe (com equipamento adequado para não morrer imediatamente), selecionar seu alvo e conferir HP/ATK/DEF/HIT/FLEE; subir de nível em combate ou missão, conferir o requisito EXP crescente e a atualização do máximo de HP/SP; verificar se esquivas produzem `MISS`. Lembre-se de copiar o save antes.
