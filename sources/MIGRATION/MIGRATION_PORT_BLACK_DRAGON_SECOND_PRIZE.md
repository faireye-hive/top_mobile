# Black Dragon Second Prize — porte parcial

O callback histórico `ItemUse_erdengjiang` (ID 274) está em
`Client/scripts/offline/script/calculate/ItemEffect.lua`. Ele exige 14 espaços,
entrega 10 Strengthening Crystals (456) de qualidade 4 e sorteia igualmente um
dos oito conjuntos de três ou quatro peças (IDs 5013..5038), também de
qualidade 4.

O OpenGL usa o executor transacional já existente em `ItemChestRules`:
a bolsa só é atualizada depois que todas as recompensas do resultado cabem e
estão presentes no catálogo local. O pacote atual não define os IDs 5034..5038;
os resultados 4, 6, 7 e 8 que dependem deles preservam o baú e não entregam
parcialmente as recompensas. Os outros quatro resultados podem ser abertos.

O estado permanece `PORT_PARTIAL`, não concluído. Para completar o callback,
será necessário recuperar definições históricas válidas para 5034..5038 e
adicioná-las ao catálogo antes de habilitar esses quatro resultados. O sorteio
usa o gerador local, portanto não replica a sequência global de `math.random`
do servidor.
