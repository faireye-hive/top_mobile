# Porte do callback `ItemUse_BirthCake`

Os itens históricos 3240 (`Almighty Gem`) e 3338 (`Birthday Cake`) agora
acrescentam 3 pontos de atributo ao total livre do personagem, conforme
`ItemEffect.lua`. O uso no barco é recusado sem consumir o item. O cliente
também preserva o item quando o contador offline alcança seu limite seguro,
em vez de estourar o valor persistido. Esse teto local não aparece no script
histórico e permanece marcado como `PORT_APPROX` até comparar o limite do
servidor.

O save já existente contém `ap_`; não foi criado nem alterado nenhum efeito de
skill. `Play::selfTest` cobre o ganho de três pontos com save/reload, a recusa
no barco e o limite do contador. O auditor marca os dois itens como
`birth_cake_PORT_APPROX`.
