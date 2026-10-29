# Migração 05-BT — ficha jogável baseada em AttrCalculate.lua

`characterinfo.txt` fornece atributos de modelos/atores (incluindo o Newbie usado como modelo), não uma ficha inicial completa de personagem jogável. Antes, a ficha offline somava os coeficientes da classe a esses valores e podia começar com HP, ataque e defesa abaixo do que a fórmula histórica determina.

`Play::applyAttrs` agora recalcula o baseline Newbie e os derivados da classe a partir de `Client/scripts/offline/script/calculate/AttrCalculate.lua`, usando nível e atributos permanentes. Isso cobre HP máximo, SP máximo, ataque mínimo/máximo e defesa; bônus primários de equipamento entram antes do cálculo, e os bônus diretos continuam sendo aplicados depois. A remoção do equipamento recalcula a mesma ficha sem seus bônus. O HP/SP atuais são preservados e limitados aos novos máximos — não há cura automática ao equipar/remover.

O recálculo migra saves existentes em memória na próxima atualização da ficha; não exige apagar ou editar arquivos de personagem. O formato de save continua inalterado.

O autoteste verifica a ficha Newbie nível 1 (`5` em cada atributo: 100 HP, 23 SP, 7 ataque e 2 DEF), diferenças entre classes, gasto de atributos e o ciclo de equipar/remover. Isso é uma correção da base de atributos, não certifica ainda todos os detalhes de progressão, críticos, passivas ou fórmulas avançadas do servidor histórico.
