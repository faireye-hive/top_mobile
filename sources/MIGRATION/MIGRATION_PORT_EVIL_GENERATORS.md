# Porte dos Evil Generators

Os callbacks históricos `ItemUse_FaSheng1..4` estão associados aos itens
1144, 2728, 2752 e 2776. Eles recusam uso no barco e em zona segura, sorteiam
um monstro de uma lista fixa e chamam `CreateChaX` a cinco unidades de mapa do
jogador. Os níveis 1 e 2 duram 3600 segundos; o nível 3 dura 7200; o nível 4
dura 10800 para o monstro 952 e 7200 para os demais. Os valores de refresh
históricos são preservados como 3700, 7300 ou 10900 segundos.

O OpenGL agora lê os mesmos pools de `ItemEffect.lua`, carrega as fichas desses
monstros de `characterinfo.txt`, cria as instâncias pelo invocador histórico
existente e só consome o item depois que a criação tem sucesso. Todos os IDs
dos pools foram encontrados na tabela de personagens. O combate dos monstros
usa as fichas e habilidades carregadas que já existem; nenhum efeito de skill
foi alterado.

## Aproximação e verificação

- `Play::roll` mantém a escolha reproduzível do cliente offline e não reproduz
  a sequência compartilhada de `math.random` do DX9.
- `top_gl` compilou após a integração. A execução no cliente e a observação do
  combate de cada pool ainda precisam de validação dentro do jogo; por isso a
  classificação do PORT-00 permanece D1 até haver essa evidência.
