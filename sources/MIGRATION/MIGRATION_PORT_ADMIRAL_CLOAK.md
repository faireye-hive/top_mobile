# PORT: Admiral Cloak Chest

## Regra historica

`Client/scripts/offline/script/calculate/ItemEffect.lua::ItemUse_Admiral` recusa uso no barco, exige dois espacos livres e concede `Server.Cloak.Item.ID` (9604) com argumento de qualidade 88. `variable.lua` define esse ID. O callback tambem bloqueia inventario travado e estados `STATE_BAT`/`STATE_JY`; esses estados de servidor nao existem no cliente offline.

## Fluxo OpenGL

O item 9603 agora usa uma transacao local: verifica barco, espaco e catalogo do item 9604; simula consumo do bau e insercao da capa antes de alterar a bolsa; registra a recompensa, salva e preserva o bau em qualquer falha. A verificacao indisponivel de inventario travado/estados online esta marcada como aproximacao.

## Cobertura

`PORT_APPROX` ate validar uso visual e save/reload no jogo. O verificador de acoes de itens e `ITEM_ACTIONS_05-AW.csv` classificam o item como `admiral_cloak_chest_PORT_APPROX`. Nenhuma regra ou efeito de skill foi alterado; nenhuma missao foi criada.
