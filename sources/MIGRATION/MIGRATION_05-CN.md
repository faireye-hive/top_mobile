# Migração 05-CN — Famous Cake e reputação

## Regra histórica

`ItemUse_NiceCake` exige um Medal of Valor (3849) na bolsa e só funciona quando
`VAL_STR` do medal está abaixo de 27000 e `ATTR_FAME` do personagem abaixo de
99990001. No sucesso, soma 3000 ao honor do medal, soma 9999 à reputação e
consome o bolo 2988. Se faltar medal ou algum limite for alcançado, o bolo é
preservado.

## Porte OpenGL

O handler aplica as duas alterações na mesma transação local da bolsa e grava o
progresso. Como o modelo offline não tinha `ATTR_FAME`, foi acrescentado o campo
`fame_` e o sufixo opcional `FM1` ao save. Saves antigos continuam carregando
com reputação zero; o novo campo não desloca os campos posicionais anteriores.

## Verificação e limites

- Fontes: `ItemEffect.lua::ItemUse_NiceCake`, `iteminfo.txt::2988/3849` e
  `ATTR_FAME` usado pelo callback histórico.
- Estado D1: compilação, uso interativo e reload do campo FM1 ainda precisam de
  validação. Outras fontes históricas que aumentam `ATTR_FAME` não foram
  conectadas nesta alteração.
