# 05-Z4 — transações duráveis do cofre offline

O cofre já cobrava a taxa histórica, movia itens e salvava banco/inventário,
mas ignorava falhas de gravação. Isso permitia debitar a taxa ou transferir um
item somente na memória, com o save antigo intacto. Agora abertura, depósito e
retirada só confirmam a operação se `save()` retornar sucesso; nas falhas antes
do commit, restauram ouro, abertura, bolsa, cofre e vínculos de UID.

As regressões usam uma pasta temporária exclusiva para validar a persistência
normal e um destino sem escrita para provar o rollback dos três fluxos. O
arquivo principal e seu `.bak` de teste são removidos ao final. Falha no
`fsync` do diretório depois do `rename` continua sendo um caso de durabilidade
incerta a cobrir com injeção de falhas. O teste de estado
`ui05v_play_state_regression` passou.
