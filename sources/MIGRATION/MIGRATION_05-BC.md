# 05-BC — estado físico de instâncias de item

## O que foi portado nesta etapa

- `Play::ItemInfo` passa a ler `chInstance` (coluna 21) e energia máxima (coluna 75) de `iteminfo.txt`; antes somente a durabilidade máxima (coluna 76) era conservada.
- `Stack` agora pode guardar qualidade, durabilidade atual/máxima, energia atual/máxima e negociabilidade sem misturar esses valores aos bônus fixos do item.
- O save escreve instâncias no formato `Y2/T2/BI2`. O leitor mantém suporte a `Y1/T1/BI1`, e os campos novos ficam em seus valores neutros quando um save antigo não os contém.
- O fluxo posicional antigo do personagem e as tags de forja continuam inalterados.

## Limite funcional ainda aberto

Ler e salvar esses campos não significa que todos os itens instanciáveis já são gerados. O legado chama `CFightAble::ItemInstance` com origens distintas (`BUY`, `MONS`, `COMP`, `TASK`) e só executa `Creat_Item` quando `chInstance` está marcado. As regras portadas em 05-BA cobrem eventos Lua 11 e 95, mas ainda não identificam nem reproduzem todos os eventos/origens. Portanto, esta etapa não inventa durabilidade, energia ou afixos para recompensas e não habilita a caixa 443.

Referências locais: `arquivo/fontes/Server/GameServer/src/FightAble.cpp` (`CFightAble::ItemInstance`), `arquivo/fontes/Server/GameServer/src/Character.cpp` (chamadas por origem) e `Client/scripts/offline/script/calculate/AttrCalculate.lua` (`Creat_Item`).

## Verificação

`top_ui05ba_tests` cobre round-trip Y2 e leitura Y1; a suíte CTest completa e `top_gl --check` passaram nesta revisão.
